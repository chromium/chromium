// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_lock_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kLockButtonCornerRadius = 1;

}  // namespace

SnapGroupLockButton::SnapGroupLockButton(aura::Window* window1,
                                         aura::Window* window2)
    : ImageButton(base::BindRepeating(&SnapGroupLockButton::OnLockButtonPressed,
                                      base::Unretained(this),
                                      window1,
                                      window2)) {
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);

  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  const bool locked =
      snap_group_controller->AreWindowsInSnapGroup(window1, window2);
  UpdateLockButtonIcon(locked);
  UpdateLockButtonTooltip(locked);

  SetBackground(views::CreateThemedRoundedRectBackground(
      kColorAshShieldAndBase80, kLockButtonCornerRadius));
}

SnapGroupLockButton::~SnapGroupLockButton() = default;

void SnapGroupLockButton::OnLockButtonPressed(aura::Window* window1,
                                              aura::Window* window2) {
  DCHECK(window1);
  DCHECK(window2);
  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  const bool locked =
      snap_group_controller->AreWindowsInSnapGroup(window1, window2);

  if (locked) {
    snap_group_controller->RemoveSnapGroupContainingWindow(window1);
  } else {
    snap_group_controller->AddSnapGroup(window1, window2);
  }

  UpdateLockButtonIcon(!locked);
  UpdateLockButtonTooltip(!locked);
}

void SnapGroupLockButton::UpdateLockButtonIcon(bool locked) {
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(locked ? kLockScreenEasyUnlockCloseIcon
                                            : kLockScreenEasyUnlockOpenIcon,
                                     kColorAshIconColorPrimary));
}

void SnapGroupLockButton::UpdateLockButtonTooltip(bool locked) {
  SetTooltipText(l10n_util::GetStringUTF16(
      locked ? IDS_ASH_SNAP_GROUP_CLICK_TO_UNLOCK_WINDOWS
             : IDS_ASH_SNAP_GROUP_CLICK_TO_LOCK_WINDOWS));
}

BEGIN_METADATA(SnapGroupLockButton, views::ImageButton)
END_METADATA

}  // namespace ash