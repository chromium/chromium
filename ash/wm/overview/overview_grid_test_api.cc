// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid_test_api.h"

#include "ash/wm/overview/overview_test_util.h"

namespace ash {

OverviewGridTestApi::OverviewGridTestApi(OverviewGrid* overview_grid)
    : overview_grid_(overview_grid) {}

OverviewGridTestApi::OverviewGridTestApi(aura::Window* root)
    : overview_grid_(GetOverviewGridForRoot(root)) {}

OverviewGridTestApi::~OverviewGridTestApi() = default;

const std::vector<raw_ptr<BirchChipButtonBase>>&
OverviewGridTestApi::GetBirchChips() const {
  return overview_grid_->birch_bar_view_->chips_;
}

}  // namespace ash
