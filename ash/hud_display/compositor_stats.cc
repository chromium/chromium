// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/compositor_stats.h"

#include "ui/compositor/compositor.h"
#include "ui/gfx/presentation_feedback.h"

namespace ash {
namespace hud_display {

CompositorStats::CompositorStats() = default;
CompositorStats::~CompositorStats() = default;

void CompositorStats::OnDidPresentCompositorFrame(
    const gfx::PresentationFeedback& feedback) {
  constexpr base::TimeDelta kOneSec = base::TimeDelta::FromSeconds(1);
  constexpr base::TimeDelta k500ms = base::TimeDelta::FromMilliseconds(500);
  if (!feedback.failed())
    presented_times_.push_back(feedback.timestamp);

  const base::TimeTicks deadline_1s = feedback.timestamp - kOneSec;
  while (!presented_times_.empty() && presented_times_.front() <= deadline_1s)
    presented_times_.pop_front();

  const base::TimeTicks deadline_500ms = feedback.timestamp - k500ms;
  frame_rate_for_last_half_second_ = 0;
  for (auto i = presented_times_.crbegin();
       (i != presented_times_.crend()) && (*i > deadline_500ms); ++i) {
    ++frame_rate_for_last_half_second_;
  }
  frame_rate_for_last_half_second_ *= 2;
}

}  // namespace hud_display
}  // namespace ash
