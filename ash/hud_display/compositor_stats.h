// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_COMPOSITOR_STATS_H_
#define ASH_HUD_DISPLAY_COMPOSITOR_STATS_H_

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "ui/compositor/compositor_observer.h"

namespace ui {
class Compositor;
}
namespace gfx {
struct PresentationFeedback;
}

namespace ash {
namespace hud_display {

class CompositorStats : public ui::CompositorObserver {
 public:
  class Observer {
   public:
    Observer() = default;
    virtual ~Observer() = default;

    virtual void OnFramePresented(float frame_rate_1s,
                                  float frame_rate_500ms,
                                  float refresh_rate) = 0;
  };

  CompositorStats(Observer* observer, ui::Compositor* compositor);
  ~CompositorStats() override;

  // ui::CompositorObserver:
  void OnDidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override;

 private:
  Observer* const observer_;
  ui::Compositor* const compositor_;

  // |timestamp| from PresentationFeedback for one second.
  base::circular_deque<base::TimeTicks> presented_times_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_COMPOSITOR_STATS_H_
