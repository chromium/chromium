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

}  // namespace ash
