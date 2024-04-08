// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_view.h"

#include <memory>
#include <utility>

#include "ash/picker/views/picker_focus_indicator.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/style/style_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr auto kPickerItemFocusIndicatorMargins = gfx::Insets::VH(6, 0);

std::unique_ptr<views::Background> GetPickerItemBackground(
    PickerItemView::ItemState item_state,
    int corner_radius) {
  switch (item_state) {
    case PickerItemView::ItemState::kNormal:
      return nullptr;
    case PickerItemView::ItemState::kPseudoFocused:
      return views::CreateThemedRoundedRectBackground(
          cros_tokens::kCrosSysHoverOnSubtle, corner_radius);
  }
}

}  // namespace

PickerItemView::PickerItemView(SelectItemCallback select_item_callback,
                               FocusIndicatorStyle focus_indicator_style)
    : views::Button(select_item_callback),
      select_item_callback_(select_item_callback),
      focus_indicator_style_(focus_indicator_style) {
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
  views::InkDrop::Get(this)->GetInkDrop()->SetHoverHighlightFadeDuration(
      base::TimeDelta());

  switch (focus_indicator_style_) {
    case FocusIndicatorStyle::kFocusRing:
      views::FocusRing::Get(this)->SetHasFocusPredicate(
          base::BindRepeating([](const View* view) {
            const auto* v = views::AsViewClass<PickerItemView>(view);
            CHECK(v);
            return (v->HasFocus() ||
                    v->GetItemState() ==
                        PickerItemView::ItemState::kPseudoFocused);
          }));
      break;
    case FocusIndicatorStyle::kFocusBar:
      // Disable default focus ring to use a custom focus indicator.
      SetInstallFocusRingOnFocus(false);
      break;
  }
}

PickerItemView::~PickerItemView() {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->CloseBubble();
  }
}

void PickerItemView::SetPreview(
    PickerPreviewBubbleController* preview_bubble_controller) {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->CloseBubble();
  }

  preview_bubble_controller_ = preview_bubble_controller;
}

void PickerItemView::PaintButtonContents(gfx::Canvas* canvas) {
  views::Button::PaintButtonContents(canvas);

  if (focus_indicator_style_ == FocusIndicatorStyle::kFocusBar &&
      item_state_ == ItemState::kPseudoFocused) {
    PaintPickerFocusIndicator(
        canvas, gfx::Point(0, kPickerItemFocusIndicatorMargins.top()),
        height() - kPickerItemFocusIndicatorMargins.height(),
        GetColorProvider()->GetColor(cros_tokens::kCrosSysFocusRing));
  }
}

void PickerItemView::SelectItem() {
  select_item_callback_.Run();
}

void PickerItemView::OnMouseEntered(const ui::MouseEvent&) {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->ShowBubble(this);
  }
}

void PickerItemView::OnMouseExited(const ui::MouseEvent&) {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->CloseBubble();
  }
}

void PickerItemView::SetCornerRadius(int corner_radius) {
  if (corner_radius_ == corner_radius) {
    return;
  }

  corner_radius_ = corner_radius;
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, gfx::RoundedCornersF(corner_radius_));
  SetBackground(GetPickerItemBackground(item_state_, corner_radius_));
}

PickerItemView::ItemState PickerItemView::GetItemState() const {
  return item_state_;
}

void PickerItemView::SetItemState(ItemState item_state) {
  if (item_state_ == item_state) {
    return;
  }

  item_state_ = item_state;
  SetBackground(GetPickerItemBackground(item_state_, corner_radius_));
  switch (focus_indicator_style_) {
    case FocusIndicatorStyle::kFocusRing:
      views::FocusRing::Get(this)->SchedulePaint();
      break;
    case FocusIndicatorStyle::kFocusBar:
      SchedulePaint();
      break;
  }
}

BEGIN_METADATA(PickerItemView)
END_METADATA

}  // namespace ash
