// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/raster_query_queue.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "cc/base/histograms.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"

namespace cc {

RasterQuery::RasterQuery() = default;

RasterQuery::~RasterQuery() = default;

RasterQueryQueue::RasterQueryQueue(
    viz::RasterContextProvider* const worker_context_provider)
    : worker_context_provider_(worker_context_provider) {}

RasterQueryQueue::~RasterQueryQueue() = default;

void RasterQueryQueue::Append(RasterQuery raster_query) {
  // It is important for this method to not be called with the raster context
  // lock to avoid a deadlock in CheckRasterFinishedQueries, which acquired
  // the raster context lock while holding this lock.
  base::AutoLock hold(pending_raster_queries_lock_);
  pending_raster_queries_.push_back(std::move(raster_query));
}

#define UMA_HISTOGRAM_RASTER_TIME_CUSTOM_MICROSECONDS(name, total_time) \
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(                              \
      name, total_time, base::Microseconds(1), base::Milliseconds(100), 100);

bool RasterQueryQueue::CheckRasterFinishedQueries() {
  base::AutoLock hold(pending_raster_queries_lock_);
  if (pending_raster_queries_.empty())
    return false;

  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_);
  auto* ri = scoped_context.RasterInterface();

  auto it = pending_raster_queries_.begin();
  while (it != pending_raster_queries_.end()) {
    GLuint complete = 0;
    ri->GetQueryObjectuivEXT(it->raster_duration_query_id,
                             GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT,
                             &complete);
    if (!complete)
      break;

#if DCHECK_IS_ON()
    if (it->raster_start_query_id) {
      // We issued the GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM query prior to the
      // GL_COMMANDS_ISSUED_CHROMIUM query. Therefore, if the result of the
      // latter is available, the result of the former should be too.
      complete = 0;
      ri->GetQueryObjectuivEXT(it->raster_start_query_id,
                               GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT,
                               &complete);
      DCHECK(complete);
    }
#endif

    GLuint gpu_raster_duration = 0u;
    ri->GetQueryObjectuivEXT(it->raster_duration_query_id, GL_QUERY_RESULT_EXT,
                             &gpu_raster_duration);
    ri->DeleteQueriesEXT(1, &it->raster_duration_query_id);

    base::TimeDelta raster_duration =
        it->worker_raster_duration + base::Microseconds(gpu_raster_duration);

    // It is safe to use the UMA macros here with runtime generated strings
    // because the client name should be initialized once in the process, before
    // recording any metrics here.
    const char* client_name = GetClientNameForMetrics();

    if (it->raster_start_query_id) {
      GLuint64 gpu_raster_start_time = 0u;
      ri->GetQueryObjectui64vEXT(it->raster_start_query_id, GL_QUERY_RESULT_EXT,
                                 &gpu_raster_start_time);
      ri->DeleteQueriesEXT(1, &it->raster_start_query_id);

      // The base::checked_cast<int64_t> should not crash as long as the GPU
      // process was not compromised: that's because the result of the query
      // should have been generated using base::TimeDelta::InMicroseconds()
      // there, so the result should fit in an int64_t.
      base::TimeDelta raster_scheduling_delay =
          base::Microseconds(
              base::checked_cast<int64_t>(gpu_raster_start_time)) -
          it->raster_buffer_creation_time.since_origin();

      // We expect the clock we're using to be monotonic, so we shouldn't get a
      // negative scheduling delay.
      DCHECK_GE(raster_scheduling_delay.InMicroseconds(), 0u);
      UMA_HISTOGRAM_RASTER_TIME_CUSTOM_MICROSECONDS(
          base::StringPrintf(
              "Renderer4.%s.RasterTaskSchedulingDelayNoAtRasterDecodes.All",
              client_name),
          raster_scheduling_delay);
      if (it->depends_on_hardware_accelerated_jpeg_candidates) {
        UMA_HISTOGRAM_RASTER_TIME_CUSTOM_MICROSECONDS(
            base::StringPrintf(
                "Renderer4.%s.RasterTaskSchedulingDelayNoAtRasterDecodes."
                "TilesWithJpegHwDecodeCandidates",
                client_name),
            raster_scheduling_delay);
      }
      if (it->depends_on_hardware_accelerated_webp_candidates) {
        UMA_HISTOGRAM_RASTER_TIME_CUSTOM_MICROSECONDS(
            base::StringPrintf(
                "Renderer4.%s.RasterTaskSchedulingDelayNoAtRasterDecodes."
                "TilesWithWebPHwDecodeCandidates",
                client_name),
            raster_scheduling_delay);
      }
    }

    UMA_HISTOGRAM_RASTER_TIME_CUSTOM_MICROSECONDS(
        base::StringPrintf("Renderer4.%s.RasterTaskTotalDuration.Oop",
                           client_name),
        raster_duration);

    it = pending_raster_queries_.erase(it);
  }

  return pending_raster_queries_.size() > 0u;
}

}  // namespace cc
