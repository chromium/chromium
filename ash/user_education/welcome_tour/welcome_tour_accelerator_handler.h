// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_ACCELERATOR_HANDLER_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_ACCELERATOR_HANDLER_H_

#include <array>

#include "ash/public/cpp/accelerator_actions.h"
#include "base/functional/callback.h"
#include "ui/events/event_handler.h"

namespace ash {

// Handles the accelerator key events during the Welcome Tour.
class WelcomeTourAcceleratorHandler : public ui::EventHandler {
 public:
  struct AllowedAction {
    const AcceleratorAction action;

    // If true, when `action` is triggered during the Welcome Tour, the tour is
    // aborted and `action` continues to perform as usual.
    const bool aborts_tour;
  };

  // The accelerator actions allowed during the Welcome Tour.
  static constexpr std::array<AllowedAction, 13> kAllowedActions = {{
      {AcceleratorAction::kBrightnessDown, /*aborts_tour=*/false},
      {AcceleratorAction::kBrightnessUp, /*aborts_tour=*/false},
      {AcceleratorAction::kExit, /*aborts_tour=*/true},
      {AcceleratorAction::kLockScreen, /*aborts_tour=*/true},
      {AcceleratorAction::kPowerPressed, /*aborts_tour=*/true},
      {AcceleratorAction::kPowerReleased, /*aborts_tour=*/true},
      {AcceleratorAction::kPrintUiHierarchies, /*aborts_tour=*/false},
      {AcceleratorAction::kSuspend, /*aborts_tour=*/true},
      {AcceleratorAction::kTakeScreenshot, /*aborts_tour=*/false},
      {AcceleratorAction::kToggleSpokenFeedback, /*aborts_tour=*/true},
      {AcceleratorAction::kVolumeDown, /*aborts_tour=*/false},
      {AcceleratorAction::kVolumeMute, /*aborts_tour=*/false},
      {AcceleratorAction::kVolumeUp, /*aborts_tour=*/false},
  }};

  explicit WelcomeTourAcceleratorHandler(
      base::RepeatingClosure abort_tour_callback);
  WelcomeTourAcceleratorHandler(const WelcomeTourAcceleratorHandler&) = delete;
  WelcomeTourAcceleratorHandler& operator=(
      const WelcomeTourAcceleratorHandler&) = delete;
  ~WelcomeTourAcceleratorHandler() override;

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // The callback to abort the Welcome Tour.
  base::RepeatingClosure abort_tour_callback_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_ACCELERATOR_HANDLER_H_
