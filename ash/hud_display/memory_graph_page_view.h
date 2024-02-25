// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_MEMORY_GRAPH_PAGE_VIEW_H_
#define ASH_HUD_DISPLAY_MEMORY_GRAPH_PAGE_VIEW_H_

#include "ash/hud_display/graph.h"
#include "ash/hud_display/graph_page_view_base.h"
#include "base/memory/raw_ptr.h"

namespace ash {
namespace hud_display {

class ReferenceLines;

// MemoryGraphPageView class draws memory graphs.
class MemoryGraphPageView : public GraphPageViewBase {
  METADATA_HEADER(MemoryGraphPageView, GraphPageViewBase)

 public:
  explicit MemoryGraphPageView(const base::TimeDelta refresh_interval);
  MemoryGraphPageView(const MemoryGraphPageView&) = delete;
  MemoryGraphPageView& operator=(const MemoryGraphPageView&) = delete;
  ~MemoryGraphPageView() override;

  // view::
  void OnPaint(gfx::Canvas* canvas) override;

  // Update page data from the new snapshot.
  void UpdateData(const DataSource::Snapshot& snapshot) override;

 private:
  // This is used to re-layout reference lines when total ram size is known.
  double total_ram_ = 0;

  // --- Stacked:
  // Share of the total RAM occupied by Chrome browser private RSS.
  Graph graph_chrome_rss_private_;
  // Share of the total RAM reported as Free memory be kernel.
  Graph graph_mem_free_;
  // Total RAM - other graphs in this stack.
  Graph graph_mem_used_unknown_;
  // Share of the total RAM occupied by Chrome type=renderer processes private
  // RSS.
  Graph graph_renderers_rss_private_;
  // Share of the total RAM occupied by ARC++ processes private RSS.
  Graph graph_arc_rss_private_;
  // Share of the total RAM occupied by Chrome type=gpu process private RSS.
  Graph graph_gpu_rss_private_;
  // Share of the total RAM used by kernel GPU driver.
  Graph graph_gpu_kernel_;

  // Not stacked:
  // Share of the total RAM occupied by Chrome browser process shared RSS.
  Graph graph_chrome_rss_shared_;

  raw_ptr<ReferenceLines> reference_lines_ = nullptr;  // not owned.
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_MEMORY_GRAPH_PAGE_VIEW_H_
