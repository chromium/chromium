// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/controls/contextual_nudge.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// Controller to manage keyboard backlight color education nudge.
class ASH_EXPORT KeyboardBacklightColorNudgeController {
 public:
  KeyboardBacklightColorNudgeController();

  KeyboardBacklightColorNudgeController(
      const KeyboardBacklightColorNudgeController&) = delete;
  KeyboardBacklightColorNudgeController& operator=(
      const KeyboardBacklightColorNudgeController&) = delete;

  ~KeyboardBacklightColorNudgeController();

  // Determines whether the education nudge for wallpaper extracted color in the
  // personalization hub can be shown.
  static bool ShouldShowWallpaperColorNudge();

  // Called when the wallpaper color nudge is shown.
  static void HandleWallpaperColorNudgeShown();

  // Determines whether the education nudge can be shown and shows it.
  void MaybeShowEducationNudge(views::View* keyboard_brightness_slider_view);

  void CloseEducationNudge();

  // Called when the user has manually set the color.
  void SetUserPerformedAction();

 private:
  // Starts auto close timer.
  void StartAutoCloseTimer();

  base::OneShotTimer autoclose_;

  raw_ptr<ContextualNudge, DanglingUntriaged> education_nudge_ = nullptr;

  base::WeakPtrFactory<KeyboardBacklightColorNudgeController> weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_KEYBOARD_BACKLIGHT_COLOR_NUDGE_CONTROLLER_H_
