// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kButtonMargin = 2;
constexpr int kButtonSpacing = 4;
constexpr int kCornerRadius = 11;

}  // namespace

DeskActionView::DeskActionView(base::RepeatingClosure combine_desks_callback,
                               base::RepeatingClosure close_all_callback)
    : close_all_button_(AddChildView(
          std::make_unique<CloseButton>(std::move(close_all_callback),
                                        CloseButton::Type::kMediumFloating))),
      combine_desks_button_(AddChildView(
          std::make_unique<CloseButton>(std::move(combine_desks_callback),
                                        CloseButton::Type::kMediumFloating))) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetInsideBorderInsets(gfx::Insets(kButtonMargin));
  SetBetweenChildSpacing(kButtonSpacing);
  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80),
      kCornerRadius));

  // TODO(crbug.com/1302030): Localize the strings here.
  close_all_button_->SetTooltipText(u"Close desk and windows");
  combine_desks_button_->SetVectorIcon(kCombineDesksIcon);
  combine_desks_button_->SetTooltipText(u"Combine with ");
}

BEGIN_METADATA(DeskActionView, views::BoxLayoutView)
END_METADATA

}  // namespace ash