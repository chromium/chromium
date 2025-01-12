// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_container_view.h"

#include <memory>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

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

BEGIN_METADATA(ActionButtonContainerView)
END_METADATA

}  // namespace ash
