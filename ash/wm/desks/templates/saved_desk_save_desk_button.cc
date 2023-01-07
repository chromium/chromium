// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/highlight_border.h"

namespace ash {

constexpr int kCornerRadius = 16;

SavedDeskSaveDeskButton::SavedDeskSaveDeskButton(
    base::RepeatingClosure callback,
    const std::u16string& text,
    Type button_type,
    const gfx::VectorIcon* icon)
    : PillButton(callback,
                 text,
                 PillButton::Type::kDefaultWithIconLeading,
                 icon),
      callback_(callback),
      button_type_(button_type) {
  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<SavedDeskSaveDeskButton*>(view)->IsViewHighlighted();
  });
  focus_ring->SetColorId(ui::kColorAshFocusRing);

  if (features::IsDarkLightModeEnabled()) {
    SetBorder(std::make_unique<views::HighlightBorder>(
        /*corner_radius=*/kCornerRadius,
        views::HighlightBorder::Type::kHighlightBorder2,
        /*use_light_colors=*/false));
  }
}

SavedDeskSaveDeskButton::~SavedDeskSaveDeskButton() = default;

views::View* SavedDeskSaveDeskButton::GetView() {
  return this;
}

void SavedDeskSaveDeskButton::MaybeActivateHighlightedView() {
  if (GetEnabled())
    callback_.Run();
}

void SavedDeskSaveDeskButton::MaybeCloseHighlightedView(bool primary_action) {}

void SavedDeskSaveDeskButton::MaybeSwapHighlightedView(bool right) {}

void SavedDeskSaveDeskButton::OnViewHighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void SavedDeskSaveDeskButton::OnViewUnhighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void SavedDeskSaveDeskButton::OnThemeChanged() {
  PillButton::OnThemeChanged();
  SetBackgroundColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));
}

void SavedDeskSaveDeskButton::OnFocus() {
  UpdateOverviewHighlightForFocusAndSpokenFeedback(this);
  OnViewHighlighted();
  View::OnFocus();
}

void SavedDeskSaveDeskButton::OnBlur() {
  OnViewUnhighlighted();
  View::OnBlur();
}

BEGIN_METADATA(SavedDeskSaveDeskButton, PillButton)
END_METADATA

}  // namespace ash
