// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/graphs_container_view.h"

#include <numeric>

#include "ash/hud_display/cpu_graph_page_view.h"
#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/memory_graph_page_view.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace hud_display {
namespace {

// UI refresh interval.
constexpr base::TimeDelta kGraphsDataRefreshInterval =
    base::TimeDelta::FromMilliseconds(500);

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GraphsContainerView, public:

BEGIN_METADATA(GraphsContainerView, views::View)
END_METADATA

GraphsContainerView::GraphsContainerView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // Make all graph pages take the whole view and make sure that only one
  // is shown at a time.
  SetLayoutManager(std::make_unique<views::FillLayout>());

  refresh_timer_.Start(FROM_HERE, kGraphsDataRefreshInterval, this,
                       &GraphsContainerView::UpdateData);

  // Adds another graphs page.
  AddChildView(
      std::make_unique<MemoryGraphPageView>(refresh_timer_.GetCurrentDelay()))
      ->SetID(static_cast<int>(DisplayMode::MEMORY_DISPLAY));
  AddChildView(
      std::make_unique<CpuGraphPageView>(refresh_timer_.GetCurrentDelay()))
      ->SetID(static_cast<int>(DisplayMode::CPU_DISPLAY));
}

GraphsContainerView::~GraphsContainerView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

void GraphsContainerView::UpdateData() {
  const DataSource::Snapshot snapshot = data_source_.GetSnapshotAndReset();
  for (auto* child : children())
    static_cast<GraphPageViewBase*>(child)->UpdateData(snapshot);

  SchedulePaint();
}

void GraphsContainerView::SetMode(DisplayMode mode) {
  auto* selected = GetViewByID(static_cast<int>(mode));
  if (!selected) {
    DCHECK(selected);
    return;
  }
  for (auto* child : children())
    child->SetVisible(false);

  selected->SetVisible(true);
}

}  // namespace hud_display
}  // namespace ash
