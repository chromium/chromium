// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_COMPOSITOR_STATS_H_
#define ASH_HUD_DISPLAY_COMPOSITOR_STATS_H_

#include "base/containers/circular_deque.h"
#include "base/time/time.h"

namespace gfx {
struct PresentationFeedback;
}

namespace ash {
namespace hud_display {

class CompositorStats {
 public:
  CompositorStats();
  CompositorStats(const CompositorStats&) = delete;
  CompositorStats& operator=(const CompositorStats&) = delete;
  ~CompositorStats();

  float frame_rate_for_last_second() const { return presented_times_.size(); }

  float frame_rate_for_last_half_second() const {
    return frame_rate_for_last_half_second_;
  }

  // Updates the stats with |feedback|.
  void OnDidPresentCompositorFrame(const gfx::PresentationFeedback& feedback);

 private:
  float frame_rate_for_last_half_second_;

  // |timestamp| from PresentationFeedback for one second.
  base::circular_deque<base::TimeTicks> presented_times_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_COMPOSITOR_STATS_H_
