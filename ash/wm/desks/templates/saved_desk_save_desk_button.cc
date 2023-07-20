// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/overview/overview_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
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
  views::FocusRing::Get(this)->SetHasFocusPredicate(
      base::BindRepeating([](const views::View* view) {
        const auto* v = views::AsViewClass<SavedDeskSaveDeskButton>(view);
        CHECK(v);
        return v->IsViewHighlighted();
      }));

  SetBorder(std::make_unique<views::HighlightBorder>(
      kSaveDeskCornerRadius,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderNoShadow
          : views::HighlightBorder::Type::kHighlightBorder2));

  SetEnableBackgroundBlur(true);

  View* background_view = AddChildView(std::make_unique<views::View>());
  background_view->SetPaintToLayer();

  background_view->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kSaveDeskCornerRadius});
  background_view->layer()->SetBackgroundBlur(
      ColorProvider::kBackgroundBlurSigma);
  background_view->layer()->SetFillsBoundsOpaquely(false);
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

void SavedDeskSaveDeskButton::OnFocus() {
  UpdateOverviewHighlightForFocus(this);
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
