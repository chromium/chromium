// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"

#include "ash/style/style_util.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

SavedDeskSaveDeskButton::SavedDeskSaveDeskButton(
    base::RepeatingClosure callback,
    const std::u16string& text,
    Type button_type,
    const gfx::VectorIcon* icon)
    : PillButton(callback,
                 text,
                 PillButton::Type::kDefaultElevatedWithIconLeading,
                 icon),
      callback_(callback),
      button_type_(button_type) {
  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetHasFocusPredicate(
      base::BindRepeating([](const views::View* view) {
        const auto* v = views::AsViewClass<SavedDeskSaveDeskButton>(view);
        CHECK(v);
        return v->is_focused();
      }));

  SetBorder(std::make_unique<views::HighlightBorder>(
      kSaveDeskCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderNoShadow));

  SetEnableBackgroundBlur(true);
}

SavedDeskSaveDeskButton::~SavedDeskSaveDeskButton() = default;

views::View* SavedDeskSaveDeskButton::GetView() {
  return this;
}

void SavedDeskSaveDeskButton::MaybeActivateFocusedView() {
  if (GetEnabled())
    callback_.Run();
}

void SavedDeskSaveDeskButton::MaybeCloseFocusedView(bool primary_action) {}

void SavedDeskSaveDeskButton::MaybeSwapFocusedView(bool right) {}

void SavedDeskSaveDeskButton::OnFocusableViewFocused() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void SavedDeskSaveDeskButton::OnFocusableViewBlurred() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void SavedDeskSaveDeskButton::OnFocus() {
  MoveFocusToView(this);
  OnFocusableViewFocused();
  View::OnFocus();
}

void SavedDeskSaveDeskButton::OnBlur() {
  OnFocusableViewBlurred();
  View::OnBlur();
}

BEGIN_METADATA(SavedDeskSaveDeskButton, PillButton)
END_METADATA

}  // namespace ash
