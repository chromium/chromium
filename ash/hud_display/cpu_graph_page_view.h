// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_CPU_GRAPH_PAGE_VIEW_H_
#define ASH_HUD_DISPLAY_CPU_GRAPH_PAGE_VIEW_H_

#include "ash/hud_display/graph.h"
#include "ash/hud_display/graph_page_view_base.h"

namespace ash {
namespace hud_display {

// Draws CPU graphs;
class CpuGraphPageView : public GraphPageViewBase {
  METADATA_HEADER(CpuGraphPageView, GraphPageViewBase)

 public:
  explicit CpuGraphPageView(const base::TimeDelta refresh_interval);
  CpuGraphPageView(const CpuGraphPageView&) = delete;
  CpuGraphPageView& operator=(const CpuGraphPageView&) = delete;
  ~CpuGraphPageView() override;

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

  // Update page data from the new snapshot.
  void UpdateData(const DataSource::Snapshot& snapshot) override;

 private:
  // Stacked, percent of CPU ticks per interval:
  Graph cpu_other_;
  Graph cpu_system_;
  Graph cpu_user_;
  Graph cpu_idle_;
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_CPU_GRAPH_PAGE_VIEW_H_
