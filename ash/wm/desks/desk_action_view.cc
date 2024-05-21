// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/wm/desks/desk_action_button.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kCornerRadius = 20;

}  // namespace

DeskActionView::DeskActionView(const std::u16string& combine_desks_target_name,
                               const std::u16string& close_all_target_name,
                               base::RepeatingClosure combine_desks_callback,
                               base::RepeatingClosure close_all_callback,
                               base::RepeatingClosure focus_change_callback,
                               DeskMiniView* mini_view)
    : focus_change_callback_(std::move(focus_change_callback)),
      mini_view_(mini_view) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  blurred_background_ = std::make_unique<BlurredBackgroundShield>(
      this, kColorAshShieldAndBase80, ColorProvider::kBackgroundBlurSigma,
      gfx::RoundedCornersF(kCornerRadius));

  combine_desks_button_ = AddChildView(std::make_unique<DeskActionButton>(
      combine_desks_target_name, DeskActionButton::Type::kCombineDesk,
      std::move(combine_desks_callback), this));

  close_all_button_ = AddChildView(std::make_unique<DeskActionButton>(
      close_all_target_name, DeskActionButton::Type::kCloseDesk,
      std::move(close_all_callback), this));

  combine_desks_button_->AddObserver(this);
  close_all_button_->AddObserver(this);
}

DeskActionView::~DeskActionView() {
  combine_desks_button_->RemoveObserver(this);
  close_all_button_->RemoveObserver(this);
}

bool DeskActionView::ChildHasFocus() const {
  if (mini_view_->owner_bar()->type() == DeskBarViewBase::Type::kOverview &&
      !features::IsOverviewNewFocusEnabled()) {
    return combine_desks_button_->is_focused() ||
           close_all_button_->is_focused();
  }
  return combine_desks_button_->HasFocus() || close_all_button_->HasFocus();
}

void DeskActionView::OnViewFocused(views::View* observed) {
  CHECK(observed == combine_desks_button_ || observed == close_all_button_);
  OnFocusChange();
}

void DeskActionView::OnViewBlurred(views::View* observed) {
  CHECK(observed == combine_desks_button_ || observed == close_all_button_);
  OnFocusChange();
}

void DeskActionView::OnFocusChange() {
  focus_change_callback_.Run();
}

BEGIN_METADATA(DeskActionView)
END_METADATA

}  // namespace ash
