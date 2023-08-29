// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_view.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 20;
constexpr int kDeskCloseButtonSize = 24;

}  // namespace

DeskActionView::DeskActionView(
    const std::u16string& initial_combine_desks_target_name,
    base::RepeatingClosure combine_desks_callback,
    base::RepeatingClosure close_all_callback,
    base::RepeatingClosure focus_change_callback)
    : combine_desks_button_(AddChildView(
          std::make_unique<CloseButton>(std::move(combine_desks_callback),
                                        CloseButton::Type::kMediumFloating,
                                        &kCombineDesksIcon))),
      close_all_button_(AddChildView(
          std::make_unique<CloseButton>(std::move(close_all_callback),
                                        CloseButton::Type::kMediumFloating))),
      focus_change_callback_(std::move(focus_change_callback)) {
  blurred_background_ = std::make_unique<BlurredBackgroundShield>(
      this, kColorAshShieldAndBase80, ColorProvider::kBackgroundBlurSigma,
      gfx::RoundedCornersF(kCornerRadius));
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  combine_desks_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  close_all_button_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  close_all_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_CLOSE_ALL_DESCRIPTION));
  UpdateCombineDesksTooltip(initial_combine_desks_target_name);

  combine_desks_button_->SetPreferredSize(
      gfx::Size(kDeskCloseButtonSize, kDeskCloseButtonSize));
  close_all_button_->SetPreferredSize(
      gfx::Size(kDeskCloseButtonSize, kDeskCloseButtonSize));

  combine_desks_button_->AddObserver(this);
  close_all_button_->AddObserver(this);
}

DeskActionView::~DeskActionView() {
  combine_desks_button_->RemoveObserver(this);
  close_all_button_->RemoveObserver(this);
}

bool DeskActionView::ChildHasFocus() const {
  return combine_desks_button_->HasFocus() || close_all_button_->HasFocus();
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
}

void DeskActionView::OnViewFocused(views::View* observed) {
  CHECK(observed == combine_desks_button_ || observed == close_all_button_);
  focus_change_callback_.Run();
}

void DeskActionView::OnViewBlurred(views::View* observed) {
  CHECK(observed == combine_desks_button_ || observed == close_all_button_);
  focus_change_callback_.Run();
}

BEGIN_METADATA(DeskActionView, views::BoxLayoutView)
END_METADATA

}  // namespace ash