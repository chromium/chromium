// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_FPS_GRAPH_PAGE_VIEW_H_
#define ASH_HUD_DISPLAY_FPS_GRAPH_PAGE_VIEW_H_

#include "time.h"

#include "ash/hud_display/compositor_stats.h"
#include "ash/hud_display/graph.h"
#include "ash/hud_display/graph_page_view_base.h"

namespace ash {
namespace hud_display {

class Grid;

// Draws FPS graphs.
// Graph is updated in two ways:
// 1. Regular update with UpdateData() sifts graph and adds new point.
// 2. Every time OnFramePresented() is called, the last graph value is updated.
class FPSGraphPageView : public GraphPageViewBase,
                         public CompositorStats::Observer {
 public:
  METADATA_HEADER(FPSGraphPageView);

  explicit FPSGraphPageView(const base::TimeDelta refresh_interval);
  FPSGraphPageView(const FPSGraphPageView&) = delete;
  FPSGraphPageView& operator=(const FPSGraphPageView&) = delete;
  ~FPSGraphPageView() override;

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

  // CompositorStats::Observer
  void OnFramePresented(float frame_rate_1s,
                        float frame_rate_500ms,
                        float refresh_rate) override;

  // Update page data from the new snapshot.
  void UpdateData(const DataSource::Snapshot& snapshot) override;

 private:
  // Sets grid top label to the maximum of the observed refresh rate.
  void UpdateTopLabel(float f_refresh_rate);

  // Number of frames per second presented.
  Graph frame_rate_1s_;
  Graph frame_rate_500ms_;

  // Active display refresh rate.
  Graph refresh_rate_;

  Grid* grid_;  // not owned

  CompositorStats compositor_stats_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_FPS_GRAPH_PAGE_VIEW_H_
