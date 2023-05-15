// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view.h"

#include "ash/wm/desks/desk_mini_view.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

// -----------------------------------------------------------------------------
// DeskBarView:

DeskBarView::DeskBarView(aura::Window* root)
    : DeskBarViewBase(root, DeskBarViewBase::Type::kDeskButton) {}

DeskBarView::~DeskBarView() = default;

const char* DeskBarView::GetClassName() const {
  return "DeskBarView";
}

void DeskBarView::UpdateNewMiniViews(bool initializing_bar_view,
                                     bool expanding_bar_view) {
  CHECK(initializing_bar_view);
  CHECK(!expanding_bar_view);

  // This should not be called when a desk is removed.
  const auto& desks = DesksController::Get()->desks();
  CHECK_LE(mini_views_.size(), desks.size());

  UpdateDeskButtonsVisibility();

  // New mini views can be added at any index, so we need to iterate through and
  // insert new mini views in a position in `mini_views_` that corresponds to
  // their index in the `DeskController`'s list of desks.
  int mini_view_index = 0;
  for (const auto& desk : desks) {
    if (!FindMiniViewForDesk(desk.get())) {
      DeskMiniView* mini_view = scroll_view_contents_->AddChildViewAt(
          std::make_unique<DeskMiniView>(this, root_, desk.get()),
          mini_view_index);
      mini_views_.insert(mini_views_.begin() + mini_view_index, mini_view);
    }
    ++mini_view_index;
  }

  Layout();
}

}  // namespace ash
