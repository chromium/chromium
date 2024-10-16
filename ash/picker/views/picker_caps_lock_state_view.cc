// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_caps_lock_state_view.h"

#include <utility>

#include "ash/picker/views/picker_style.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/check.h"
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

}  // namespace

PickerCapsLockStateView::PickerCapsLockStateView(gfx::NativeView parent,
                                                 bool enabled,
                                                 const gfx::Rect& caret)
    : BubbleDialogDelegateView(nullptr,
                               views::BubbleBorder::Arrow::TOP_LEFT,
                               views::BubbleBorder::STANDARD_SHADOW) {
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());
  set_corner_radius(kPickerContainerBorderRadius);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(false);
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_inside_border_insets(kMargins);

  icon_view_ = AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          enabled ? kPickerCapsLockOnIcon : kPickerCapsLockOffIcon,
          cros_tokens::kCrosSysOnSurface)));

  BubbleDialogDelegateView::CreateBubble(this);

  SetBackground(views::CreateThemedRoundedRectBackground(
      kPickerContainerBackgroundColor, kPickerContainerBorderRadius));
  SetAnchorRect(caret);
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
