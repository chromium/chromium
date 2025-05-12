// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rendering_stats_instrumentation.h"

#include <stdint.h>

#include <utility>

#include "base/memory/ptr_util.h"

namespace cc {

// static
std::unique_ptr<RenderingStatsInstrumentation>
RenderingStatsInstrumentation::Create() {
  return base::WrapUnique(new RenderingStatsInstrumentation());
}

RenderingStatsInstrumentation::RenderingStatsInstrumentation()
    : record_rendering_stats_(false) {
}

RenderingStatsInstrumentation::~RenderingStatsInstrumentation() = default;

RenderingStats RenderingStatsInstrumentation::TakeImplThreadRenderingStats() {
  auto stats = std::move(impl_thread_rendering_stats_);
  impl_thread_rendering_stats_ = RenderingStats();
  return stats;
}

void RenderingStatsInstrumentation::AddVisibleContentArea(int64_t area) {
  if (!record_rendering_stats_)
    return;
  impl_thread_rendering_stats_.visible_content_area += area;
}

void RenderingStatsInstrumentation::AddApproximatedVisibleContentArea(
    int64_t area) {
  if (!record_rendering_stats_)
    return;
  impl_thread_rendering_stats_.approximated_visible_content_area += area;
}

}  // namespace cc
