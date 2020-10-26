// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/compositor_stats.h"

#include "ui/compositor/compositor.h"
#include "ui/gfx/presentation_feedback.h"

namespace ash {
namespace hud_display {

CompositorStats::CompositorStats(Observer* observer, ui::Compositor* compositor)
    : observer_(observer), compositor_(compositor) {
  compositor_->AddObserver(this);
}

CompositorStats::~CompositorStats() {
  compositor_->RemoveObserver(this);
}

void CompositorStats::OnDidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  constexpr base::TimeDelta kOneSec = base::TimeDelta::FromSeconds(1);
  constexpr base::TimeDelta k500ms = base::TimeDelta::FromMilliseconds(500);
  if (!feedback.failed())
    presented_times_.push_back(feedback.timestamp);

  const base::TimeTicks deadline_1s = feedback.timestamp - kOneSec;
  while (!presented_times_.empty() && presented_times_.front() <= deadline_1s)
    presented_times_.pop_front();

  const float frame_rate_1s = presented_times_.size();

  const base::TimeTicks deadline_500ms = feedback.timestamp - k500ms;
  float frame_rate_500ms = 0;
  for (auto i = presented_times_.crbegin();
       (i != presented_times_.crend()) && (*i > deadline_500ms); ++i) {
    ++frame_rate_500ms;
  }

  frame_rate_500ms *= 2;  // per second
  observer_->OnFramePresented(frame_rate_1s, frame_rate_500ms,
                              compositor_->refresh_rate());
}

}  // namespace hud_display
}  // namespace ash
