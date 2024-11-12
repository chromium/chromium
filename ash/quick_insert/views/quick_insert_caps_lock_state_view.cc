// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_caps_lock_state_view.h"

#include <utility>

#include "ash/quick_insert/views/quick_insert_style.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/check.h"
#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

constexpr gfx::Insets kMargins = gfx::Insets::VH(8, 12);
constexpr int kArrowGap = 16;

// The arrow direction should only follow the text direction of the input field,
// regardless of the locale.
views::BubbleBorder::Arrow GetArrowForTextDirection(
    base::i18n::TextDirection text_direction) {
  switch (text_direction) {
    case base::i18n::TextDirection::RIGHT_TO_LEFT:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::RIGHT_CENTER
                                 : views::BubbleBorder::Arrow::LEFT_CENTER;
    case base::i18n::TextDirection::LEFT_TO_RIGHT:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::LEFT_CENTER
                                 : views::BubbleBorder::Arrow::RIGHT_CENTER;
    default:
      return views::BubbleBorder::Arrow::RIGHT_CENTER;
  }
}

}  // namespace

PickerCapsLockStateView::PickerCapsLockStateView(
    gfx::NativeView parent,
    bool enabled,
    gfx::Rect caret_bounds,
    base::i18n::TextDirection text_direction)
    : BubbleDialogDelegateView(nullptr,
                               GetArrowForTextDirection(text_direction),
                               views::BubbleBorder::STANDARD_SHADOW) {
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());
  set_corner_radius(kQuickInsertContainerBorderRadius);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(false);
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_inside_border_insets(kMargins);

  icon_view_ = AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          enabled ? kQuickInsertCapsLockOnIcon : kQuickInsertCapsLockOffIcon,
          cros_tokens::kCrosSysOnSurface)));

  BubbleDialogDelegateView::CreateBubble(this);

  SetBackground(views::CreateThemedRoundedRectBackground(
      kQuickInsertContainerBackgroundColor, kQuickInsertContainerBorderRadius));

  caret_bounds.Outset(kArrowGap);
  SetAnchorRect(caret_bounds);
}

PickerCapsLockStateView::~PickerCapsLockStateView() = default;

void PickerCapsLockStateView::Close() {
  GetWidget()->Close();
}

void PickerCapsLockStateView::Show() {
  GetWidget()->Show();
}

BEGIN_METADATA(PickerCapsLockStateView)
END_METADATA

}  // namespace ash
