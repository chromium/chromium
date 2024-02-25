// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/test_holding_space_tray_child_bubble.h"

#include "ash/system/holding_space/holding_space_item_views_section.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

TestHoldingSpaceTrayChildBubble::Params::Params() = default;

TestHoldingSpaceTrayChildBubble::Params::Params(Params&& other) = default;

TestHoldingSpaceTrayChildBubble::Params::Params(
    CreateSectionsCallback create_sections_callback,
    CreatePlaceholderCallback create_placeholder_callback)
    : create_sections_callback(std::move(create_sections_callback)),
      create_placeholder_callback(std::move(create_placeholder_callback)) {}

TestHoldingSpaceTrayChildBubble::Params::~Params() = default;

TestHoldingSpaceTrayChildBubble::TestHoldingSpaceTrayChildBubble(
    HoldingSpaceViewDelegate* view_delegate,
    Params params)
    : HoldingSpaceTrayChildBubble(view_delegate), params_(std::move(params)) {}

std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>
TestHoldingSpaceTrayChildBubble::CreateSections() {
  return params_.create_sections_callback
             ? std::move(params_.create_sections_callback).Run(delegate())
             : std::vector<std::unique_ptr<HoldingSpaceItemViewsSection>>();
}

std::unique_ptr<views::View>
TestHoldingSpaceTrayChildBubble::CreatePlaceholder() {
  return params_.create_placeholder_callback
             ? std::move(params_.create_placeholder_callback).Run()
             : nullptr;
}

BEGIN_METADATA(TestHoldingSpaceTrayChildBubble)
END_METADATA

}  // namespace ash
