// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_container_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/capture_mode/action_button_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// The horizontal distance between action buttons in a row.
constexpr int kActionButtonSpacing = 10;

}  // namespace

ActionButtonContainerView::ActionButtonContainerView() {
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  box_layout->set_between_child_spacing(kActionButtonSpacing);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
}

ActionButtonContainerView::~ActionButtonContainerView() = default;

ActionButtonView* ActionButtonContainerView::AddActionButton(
    views::Button::PressedCallback callback,
    std::u16string text,
    const gfx::VectorIcon* icon,
    ActionButtonRank rank,
    ActionButtonViewID id) {
  // Collect the existing buttons and newly requested button, and sort them by
  // rank.
  std::vector<std::unique_ptr<ActionButtonView>> action_buttons;

  // Populate `action_buttons` with the existing action buttons, if any. We need
  // to copy the vector of `children()` as we will be removing buttons from the
  // original.
  views::View::Views children_copy = children();
  for (views::View* action_button : children_copy) {
    CHECK(action_button);
    action_buttons.push_back(
        RemoveChildViewT(views::AsViewClass<ActionButtonView>(action_button)));
  }

  CHECK(children().empty());

  // Add the new action button to the vector so it can also be sorted.
  auto new_action_button =
      std::make_unique<ActionButtonView>(std::move(callback), text, icon, rank);
  new_action_button->SetID(id);
  ActionButtonView* new_action_button_ptr = new_action_button.get();
  action_buttons.push_back(std::move(new_action_button));

  // Sort the buttons by rank.
  auto rank_sort = [](const std::unique_ptr<ActionButtonView>& lhs,
                      const std::unique_ptr<ActionButtonView>& rhs) {
    return lhs->rank() < rhs->rank();
  };
  sort(action_buttons.begin(), action_buttons.end(), rank_sort);

  // Re-insert the buttons into the container view in sorted order from highest
  // to lowest. Higher ranked buttons should appear to the right of lower ranked
  // buttons, so insert new buttons on the left.
  for (std::unique_ptr<ActionButtonView>& action_button : action_buttons) {
    AddChildView(std::move(action_button));
  }

  return new_action_button_ptr;
}

BEGIN_METADATA(ActionButtonContainerView)
END_METADATA

}  // namespace ash
