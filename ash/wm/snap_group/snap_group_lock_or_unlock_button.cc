// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_lock_or_unlock_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/wm/snap_group/snap_group_constants.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

SnapGroupLockOrUnlockButton::SnapGroupLockOrUnlockButton(
    aura::Window* window1,
    aura::Window* window2,
    base::RepeatingClosure pressed_callback)
    : ImageButton(std::move(pressed_callback)),
      window1_(window1),
      window2_(window2) {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  RefreshLockButton();
}

SnapGroupLockOrUnlockButton::~SnapGroupLockOrUnlockButton() = default;

void SnapGroupLockOrUnlockButton::RefreshLockButton() {
  const bool locked =
      Shell::Get()->snap_group_controller()->AreWindowsInSnapGroup(window1_,
                                                                   window2_);
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(locked ? kLockScreenEasyUnlockOpenIcon
                                            : kLockScreenEasyUnlockCloseIcon,
                                     kColorAshIconColorPrimary));
  SetTooltipText(l10n_util::GetStringUTF16(
      locked ? IDS_ASH_SNAP_GROUP_CLICK_TO_UNLOCK_WINDOWS
             : IDS_ASH_SNAP_GROUP_CLICK_TO_LOCK_WINDOWS));
}

BEGIN_METADATA(SnapGroupLockOrUnlockButton, views::ImageButton)
END_METADATA

}  // namespace ash
