// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rendering_stats_instrumentation.h"

#include <stdint.h>

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
  base::AutoLock scoped_lock(lock_);
  auto stats = impl_thread_rendering_stats_;
  impl_thread_rendering_stats_ = RenderingStats();
  return stats;
}

void RenderingStatsInstrumentation::IncrementFrameCount(int64_t count) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.frame_count += count;
}

void RenderingStatsInstrumentation::AddVisibleContentArea(int64_t area) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.visible_content_area += area;
}

void RenderingStatsInstrumentation::AddApproximatedVisibleContentArea(
    int64_t area) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.approximated_visible_content_area += area;
}

void RenderingStatsInstrumentation::AddCheckerboardedVisibleContentArea(
    int64_t area) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.checkerboarded_visible_content_area += area;
}

void RenderingStatsInstrumentation::AddCheckerboardedNeedsRecordContentArea(
    int64_t area) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.checkerboarded_needs_record_content_area += area;
}

void RenderingStatsInstrumentation::AddCheckerboardedNeedsRasterContentArea(
    int64_t area) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.checkerboarded_needs_raster_content_area += area;
}

void RenderingStatsInstrumentation::AddDrawDuration(
    base::TimeDelta draw_duration,
    base::TimeDelta draw_duration_estimate) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.draw_duration.Append(draw_duration);
  impl_thread_rendering_stats_.draw_duration_estimate.Append(
      draw_duration_estimate);
}

void RenderingStatsInstrumentation::AddBeginMainFrameToCommitDuration(
    base::TimeDelta begin_main_frame_to_commit_duration) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.begin_main_frame_to_commit_duration.Append(
      begin_main_frame_to_commit_duration);
}

void RenderingStatsInstrumentation::AddCommitToActivateDuration(
    base::TimeDelta commit_to_activate_duration,
    base::TimeDelta commit_to_activate_duration_estimate) {
  if (!record_rendering_stats_)
    return;

  base::AutoLock scoped_lock(lock_);
  impl_thread_rendering_stats_.commit_to_activate_duration.Append(
      commit_to_activate_duration);
  impl_thread_rendering_stats_.commit_to_activate_duration_estimate.Append(
      commit_to_activate_duration_estimate);
}

}  // namespace cc
