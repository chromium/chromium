// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_FPS_GRAPH_PAGE_VIEW_H_
#define ASH_HUD_DISPLAY_FPS_GRAPH_PAGE_VIEW_H_

#include "base/memory/raw_ptr.h"

#include "ash/hud_display/graph.h"
#include "ash/hud_display/graph_page_view_base.h"
#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
struct PresentationFeedback;
}

namespace ash {
namespace hud_display {

class ReferenceLines;

// Draws several graphs per UI compositor frame.  Every time
// OnDidPresentCompositorFrame() is called a new value is appended to the end
// of the graph. Every time UpdateData() is called, legend values are updated.
class FPSGraphPageView : public GraphPageViewBase,
                         public ui::CompositorObserver,
                         public views::WidgetObserver,
                         public aura::WindowObserver {
  METADATA_HEADER(FPSGraphPageView, GraphPageViewBase)

 public:
  explicit FPSGraphPageView(const base::TimeDelta refresh_interval);
  FPSGraphPageView(const FPSGraphPageView&) = delete;
  FPSGraphPageView& operator=(const FPSGraphPageView&) = delete;
  ~FPSGraphPageView() override;

  // GraphPageViewBase:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void UpdateData(const DataSource::Snapshot& snapshot) override;

  // ui::CompositorObserver:
  void OnDidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

 private:
  float frame_rate_for_last_second() const { return presented_times_.size(); }

  float frame_rate_for_last_half_second() const {
    return frame_rate_for_last_half_second_;
  }

  // Sets top reference line label to the maximum of the observed refresh rate.
  void UpdateTopLabel(float f_refresh_rate);

  // Updates the stats with |feedback|.
  void UpdateStats(const gfx::PresentationFeedback& feedback);

  // Number of frames per second presented.
  Graph frame_rate_1s_;
  Graph frame_rate_500ms_;

  // Active display refresh rate.
  Graph refresh_rate_;

  raw_ptr<ReferenceLines> reference_lines_;  // not owned

  float frame_rate_for_last_half_second_;

  // |timestamp| from PresentationFeedback for one second.
  base::circular_deque<base::TimeTicks> presented_times_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_FPS_GRAPH_PAGE_VIEW_H_
