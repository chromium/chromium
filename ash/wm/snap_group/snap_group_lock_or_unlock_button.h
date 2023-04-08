// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_LOCK_OR_UNLOCK_BUTTON_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_LOCK_OR_UNLOCK_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Contents view of the lock widget that appears below the resize widget when
// two windows are snapped. It acts as the entry point for the creating or
// removing the `SnapGroup`. This entry point is guarded by the feature flag
// `kSnapGroup`.
class SnapGroupLockOrUnlockButton : public views::ImageButton {
 public:
  METADATA_HEADER(SnapGroupLockOrUnlockButton);
  SnapGroupLockOrUnlockButton(aura::Window* window1, aura::Window* window2);
  SnapGroupLockOrUnlockButton(const SnapGroupLockOrUnlockButton&) = delete;
  SnapGroupLockOrUnlockButton& operator=(const SnapGroupLockOrUnlockButton&) =
      delete;
  ~SnapGroupLockOrUnlockButton() override;

  // Called on lock button is pressed to create or remove a snap group and
  // `RefreshLockButton()`.
  void OnLockButtonPressed();

 private:
  // Updates the lock icon and tooltip to reflect the lock button state.
  void RefreshLockButton();

  aura::Window* window1_;
  aura::Window* window2_;
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_LOCK_OR_UNLOCK_BUTTON_H_
