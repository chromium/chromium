// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_item_base.h"

#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"

namespace ash {

OverviewItemBase::OverviewItemBase(OverviewSession* overview_session,
                                   OverviewGrid* overview_grid,
                                   aura::Window* root_window)
    : root_window_(root_window),
      overview_session_(overview_session),
      overview_grid_(overview_grid) {}

OverviewItemBase::~OverviewItemBase() = default;

// static
std::unique_ptr<OverviewItemBase> OverviewItemBase::Create(
    aura::Window* window,
    OverviewSession* overview_session,
    OverviewGrid* overview_grid) {
  // TODO(b/295067715): Use switch to build different types of overview items
  // by checking whether the given `window` belongs to a snap group or not.
  return std::make_unique<OverviewItem>(window, overview_session,
                                        overview_grid);
}

}  // namespace ash
