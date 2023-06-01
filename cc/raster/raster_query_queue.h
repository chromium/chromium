// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_RASTER_QUERY_QUEUE_H_
#define CC_RASTER_RASTER_QUERY_QUEUE_H_

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace cc {

struct CC_EXPORT RasterQuery {
  RasterQuery();
  ~RasterQuery();

  // The id for querying the duration in executing the GPU side work.
  GLuint raster_duration_query_id = 0u;

  // The duration for executing the work on the raster worker thread.
  base::TimeDelta worker_raster_duration;

  // The id for querying the time at which we're about to start issuing raster
  // work to the driver.
  GLuint raster_start_query_id = 0u;

  // The time at which the raster buffer was created.
  base::TimeTicks raster_buffer_creation_time;

  // Whether the raster work depends on candidates for hardware accelerated
  // JPEG or WebP decodes.
  bool depends_on_hardware_accelerated_jpeg_candidates = false;
  bool depends_on_hardware_accelerated_webp_candidates = false;
};

class CC_EXPORT RasterQueryQueue {
 public:
  explicit RasterQueryQueue(
      viz::RasterContextProvider* const worker_context_provider);
  virtual ~RasterQueryQueue();

  // These functions should never be called with the raster context lock
  // acquired.
  void Append(RasterQuery raster_query);
  // This function is only virtual for testing purposes.
  virtual bool CheckRasterFinishedQueries();

 private:
  const raw_ptr<viz::RasterContextProvider, DanglingUntriaged>
      worker_context_provider_;

  // Note that this lock should never be acquired while holding the raster
  // context lock.
  base::Lock pending_raster_queries_lock_;
  base::circular_deque<RasterQuery> pending_raster_queries_
      GUARDED_BY(pending_raster_queries_lock_);
};

}  // namespace cc

#endif  // CC_RASTER_RASTER_QUERY_QUEUE_H_
