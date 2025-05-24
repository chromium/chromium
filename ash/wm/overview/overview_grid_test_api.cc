// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_grid_test_api.h"

#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button_container.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ui/views/view_utils.h"

namespace ash {

OverviewGridTestApi::OverviewGridTestApi(OverviewGrid* overview_grid)
    : overview_grid_(overview_grid) {}

OverviewGridTestApi::OverviewGridTestApi(aura::Window* root)
    : overview_grid_(GetOverviewGridForRoot(root)) {
  CHECK(overview_grid_);
}

OverviewGridTestApi::~OverviewGridTestApi() = default;

const std::vector<raw_ptr<BirchChipButtonBase>>&
OverviewGridTestApi::GetBirchChips() const {
  return overview_grid_->birch_bar_view_->chips_;
}

SavedDeskSaveDeskButtonContainer*
OverviewGridTestApi::GetSaveDeskButtonContainer() {
  return const_cast<SavedDeskSaveDeskButtonContainer*>(
      const_cast<const OverviewGridTestApi*>(this)
          ->GetSaveDeskButtonContainer());
}

const SavedDeskSaveDeskButtonContainer*
OverviewGridTestApi::GetSaveDeskButtonContainer() const {
  views::Widget* widget =
      overview_grid_->save_desk_button_container_widget_.get();
  return widget ? views::AsViewClass<SavedDeskSaveDeskButtonContainer>(
                      widget->GetContentsView())
                : nullptr;
}

}  // namespace ash
