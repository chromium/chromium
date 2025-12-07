// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_
#define CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_

#include <stdint.h>

#include <memory>

#include "base/synchronization/lock.h"
#include "cc/debug/rendering_stats.h"

namespace cc {

// RenderingStatsInstrumentation is shared among threads and manages conditional
// recording of rendering stats into a private RenderingStats instance.
class CC_DEBUG_EXPORT RenderingStatsInstrumentation {
 public:
  static std::unique_ptr<RenderingStatsInstrumentation> Create();
  RenderingStatsInstrumentation(const RenderingStatsInstrumentation&) = delete;
  virtual ~RenderingStatsInstrumentation();

  RenderingStatsInstrumentation& operator=(
      const RenderingStatsInstrumentation&) = delete;

  // Return copy of current impl thread rendering stats, and resets the current
  // stats.
  RenderingStats TakeImplThreadRenderingStats();

  // Read and write access to the record_rendering_stats_ flag is not locked to
  // improve performance. The flag is commonly turned off and hardly changes
  // it's value during runtime.
  bool record_rendering_stats() const { return record_rendering_stats_; }
  void set_record_rendering_stats(bool record_rendering_stats) {
    if (record_rendering_stats_ != record_rendering_stats)
      record_rendering_stats_ = record_rendering_stats;
  }

  void AddVisibleContentArea(int64_t area);
  void AddApproximatedVisibleContentArea(int64_t area);

 protected:
  RenderingStatsInstrumentation();

 private:
  RenderingStats impl_thread_rendering_stats_;

  bool record_rendering_stats_;
};

}  // namespace cc

#endif  // CC_DEBUG_RENDERING_STATS_INSTRUMENTATION_H_
