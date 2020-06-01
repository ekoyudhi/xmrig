/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018-2020 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2020 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "backend/cuda/runners/CudaKawPowRunner.h"
#include "3rdparty/libethash/data_sizes.h"
#include "backend/cuda/CudaLaunchData.h"
#include "backend/cuda/wrappers/CudaLib.h"
#include "base/io/log/Log.h"
#include "base/io/log/Tags.h"
#include "base/net/stratum/Job.h"
#include "base/tools/Chrono.h"
#include "crypto/kawpow/KPCache.h"
#include "crypto/kawpow/KPHash.h"


xmrig::CudaKawPowRunner::CudaKawPowRunner(size_t index, const CudaLaunchData &data) :
    CudaBaseRunner(index, data)
{
}


bool xmrig::CudaKawPowRunner::run(uint32_t /*startNonce*/, uint32_t *rescount, uint32_t *resnonce)
{
    return callWrapper(CudaLib::kawPowHash(m_ctx, m_jobBlob, m_target, rescount, resnonce, &m_skippedHashes));
}


bool xmrig::CudaKawPowRunner::set(const Job &job, uint8_t *blob)
{
    if (!CudaBaseRunner::set(job, blob)) {
        return false;
    }

    m_jobBlob = blob;

    const uint64_t height = job.height();
    const uint32_t epoch = height / KPHash::EPOCH_LENGTH;

    KPCache& cache = KPCache::s_cache;
    {
        std::lock_guard<std::mutex> lock(KPCache::s_cacheMutex);
        cache.init(epoch);
    }

    const uint64_t start_ms = Chrono::steadyMSecs();

    const bool result = CudaLib::kawPowPrepare(m_ctx, cache.data(), cache.size(), cache.dag_size(epoch), height, dag_sizes);

    const int64_t dt = Chrono::steadyMSecs() - start_ms;
    if (dt > 500) {
        LOG_INFO("%s " YELLOW("KawPow") " DAG for epoch " WHITE_BOLD("%u") " calculated " BLACK_BOLD("(%" PRIu64 "ms)"), Tags::nvidia(), epoch, dt);
    }

    return result;
}


void xmrig::CudaKawPowRunner::jobEarlyNotification(const Job&)
{
    CudaLib::kawPowStopHash(m_ctx);
}