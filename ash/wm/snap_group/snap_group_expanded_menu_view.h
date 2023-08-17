// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_EXPANDED_MENU_VIEW_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_EXPANDED_MENU_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace ash {

class IconButton;
class SnapGroup;

// A view that acts as the contents view of the snap group expanded menu widget
// which will show or hide on the kebab button toggled. Note that this view will
// only be available for the clamshell mode, the tablet mode is to be
// implemented.
class SnapGroupExpandedMenuView : public views::View {
 public:
  METADATA_HEADER(SnapGroupExpandedMenuView);

  explicit SnapGroupExpandedMenuView(SnapGroup* snap_group);
  SnapGroupExpandedMenuView(const SnapGroupExpandedMenuView&) = delete;
  SnapGroupExpandedMenuView& operator=(const SnapGroupExpandedMenuView&) =
      delete;
  ~SnapGroupExpandedMenuView() override;

  IconButton* swap_windows_button_for_testing() const {
    return swap_windows_button_;
  }
  IconButton* update_primary_window_button_for_testing() const {
    return update_primary_window_button_;
  }
  IconButton* update_secondary_window_button_for_testing() const {
    return update_secondary_window_button_;
  }
  IconButton* unlock_button_for_testing() const { return unlock_button_; }

 private:
  // Called when the `swap_windows_button_` is clicked to swap the two windows
  // in the snap group.
  void OnSwapWindowsButtonPressed();

  // Called when the `update_primary_window_button_` is clicked to update the
  // primary window in the snap group by invoking the overview session shown
  // on the other half of the screen.
  void OnUpdatePrimaryWindowButtonPressed();

  // Called when the `update_secondary_window_button_` is clicked. to update
  // the secondary window in the snap group by invoking the overview session
  // shown on the other half of the screen.
  void OnUpdateSecondaryWindowButtonPressed();

  // Called when the unlock button in the expaned menu is pressed to unlock the
  // snap group.
  void OnUnLockButtonPressed();

  // `snap_group_` is guaranteed to be non nullptr during `this` lifetime.
  // TODO(michelefan): revisit this comment and related logic if we decided to
  // show divider on one window snapped in clamshell.
  const raw_ptr<SnapGroup, DanglingUntriaged | ExperimentalAsh> snap_group_;

  // Child views of this class, which are listed from top to bottom (horizontal
  // screen orientation) or left to right (vertical screen orientation).
  raw_ptr<IconButton, ExperimentalAsh> swap_windows_button_;
  raw_ptr<IconButton, ExperimentalAsh> update_primary_window_button_;
  raw_ptr<IconButton, ExperimentalAsh> update_secondary_window_button_;
  raw_ptr<IconButton, ExperimentalAsh> unlock_button_;
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_EXPANDED_MENU_VIEW_H_
