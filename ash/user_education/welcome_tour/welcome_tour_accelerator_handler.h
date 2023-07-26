// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_ACCELERATOR_HANDLER_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_ACCELERATOR_HANDLER_H_

#include <array>

#include "ash/public/cpp/accelerator_actions.h"
#include "ui/events/event_handler.h"

namespace ash {

// Handles the accelerator key events during the Welcome Tour.
class WelcomeTourAcceleratorHandler : public ui::EventHandler {
 public:
  // The accelerator actions allowed during the Welcome Tour.
  static constexpr std::array<AcceleratorAction, 7> kAllowedActions = {
      AcceleratorAction::kBrightnessDown,
      AcceleratorAction::kBrightnessUp,
      AcceleratorAction::kPrintUiHierarchies,
      AcceleratorAction::kTakeScreenshot,
      AcceleratorAction::kVolumeDown,
      AcceleratorAction::kVolumeMute,
      AcceleratorAction::kVolumeUp,
  };

  WelcomeTourAcceleratorHandler();
  WelcomeTourAcceleratorHandler(const WelcomeTourAcceleratorHandler&) = delete;
  WelcomeTourAcceleratorHandler& operator=(
      const WelcomeTourAcceleratorHandler&) = delete;
  ~WelcomeTourAcceleratorHandler() override;

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_ACCELERATOR_HANDLER_H_
