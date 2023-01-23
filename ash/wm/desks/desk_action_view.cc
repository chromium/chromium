// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kButtonSpacing = 4;
constexpr int kCornerRadius = 20;

}  // namespace

DeskActionView::DeskActionView(
    const std::u16string& initial_combine_desks_target_name,
    base::RepeatingClosure combine_desks_callback,
    base::RepeatingClosure close_all_callback)
    : combine_desks_button_(AddChildView(
          std::make_unique<CloseButton>(std::move(combine_desks_callback),
                                        CloseButton::Type::kMediumFloating,
                                        &kCombineDesksIcon))),
      close_all_button_(AddChildView(
          std::make_unique<CloseButton>(std::move(close_all_callback),
                                        CloseButton::Type::kMediumFloating))) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetBetweenChildSpacing(kButtonSpacing);
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));

  close_all_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_CLOSE_ALL_DESCRIPTION));
  UpdateCombineDesksTooltip(initial_combine_desks_target_name);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));
  layer()->SetMasksToBounds(true);
}

void DeskActionView::UpdateCombineDesksTooltip(
    const std::u16string& new_combine_desks_target_name) {
  combine_desks_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_DESKS_COMBINE_DESKS_DESCRIPTION, new_combine_desks_target_name));
}

void DeskActionView::SetCombineDesksButtonVisibility(bool visible) {
  if (combine_desks_button_->GetVisible() == visible)
    return;

  combine_desks_button_->SetVisible(visible);

  // When `combine_desks_button_` is invisible, we want to make sure that there
  // is no space between the invisible `combine_desks_button_` and the
  // `close_all_button_`. Otherwise, the desk action view will appear lopsided
  // when the `combine_desks_button_` isn't visible.
  SetBetweenChildSpacing(visible ? kButtonSpacing : 0);
}

BEGIN_METADATA(DeskActionView, views::BoxLayoutView)
END_METADATA

}  // namespace ash