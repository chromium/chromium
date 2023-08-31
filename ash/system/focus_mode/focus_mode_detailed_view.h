// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {

class RoundedContainer;
class Switch;

// This view displays the focus panel settings that a user can set.
class ASH_EXPORT FocusModeDetailedView
    : public TrayDetailedView,
      public message_center::MessageCenterObserver,
      public FocusModeController::Observer {
 public:
  METADATA_HEADER(FocusModeDetailedView);

  explicit FocusModeDetailedView(DetailedViewDelegate* delegate);
  FocusModeDetailedView(const FocusModeDetailedView&) = delete;
  FocusModeDetailedView& operator=(const FocusModeDetailedView&) = delete;
  ~FocusModeDetailedView() override;

  // message_center::MessageCenterObserver:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // FocusModeController::Observer:
  void OnFocusModeChanged(bool in_focus_session) override;

 private:
  friend class FocusModeDetailedViewTest;

  // Creates the DND rounded container.
  void CreateDoNotDisturbContainer();

  // Handles clicks on the do not disturb toggle button.
  void OnDoNotDisturbToggleClicked();

  // This view contains a description of the focus session, as well as a toggle
  // button for staring/ending focus mode.
  raw_ptr<RoundedContainer> toggle_view_ = nullptr;
  // This view contains the timer view for the user to adjust the focus session
  // duration.
  raw_ptr<RoundedContainer> timer_view_ = nullptr;
  // This view contains controls for selecting the focus scene (background +
  // audio), as well as volume controls.
  raw_ptr<RoundedContainer> scene_view_ = nullptr;

  // This view contains a toggle for turning on/off DND.
  raw_ptr<RoundedContainer> do_not_disturb_view_ = nullptr;
  raw_ptr<Switch> do_not_disturb_toggle_button_ = nullptr;

  base::WeakPtrFactory<FocusModeDetailedView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_
