// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/flash_screen_controller.h"

#include "ash/color_enhancement/color_enhancement_controller.h"
#include "ash/shell.h"
#include "ui/accessibility/accessibility_features.h"

namespace ash {

namespace {

constexpr auto kNotificationTimerDelay = base::Milliseconds(300);
constexpr int kNumFlashesPerNotification = 2;

}  // namespace

FlashScreenController::FlashScreenController() {
  notification_observer_.Observe(message_center::MessageCenter::Get());
}

FlashScreenController::~FlashScreenController() {
  CancelTimer();
}

void FlashScreenController::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource display_source) {
  FlashOn();
}

void FlashScreenController::OnNotificationAdded(
    const std::string& notification_id) {
  FlashOn();
}

void FlashScreenController::FlashOn() {
  if (!::features::IsAccessibilityFlashScreenFeatureEnabled()) {
    return;
  }
  if (!enabled_) {
    // Don't flash if the pref is disabled.
    return;
  }

  // Don't start a flash if already flashing.
  if (notification_timer_.IsRunning()) {
    return;
  }
  auto* color_enhancement_controller =
      Shell::Get()->color_enhancement_controller();
  color_enhancement_controller->FlashScreenForNotification(/*show_flash=*/true,
                                                           color_);
  notification_timer_.Start(FROM_HERE, kNotificationTimerDelay, this,
                            &FlashScreenController::FlashOff);
}

void FlashScreenController::FlashOff() {
  // Turns off the flash.
  auto* color_enhancement_controller =
      Shell::Get()->color_enhancement_controller();
  color_enhancement_controller->FlashScreenForNotification(
      /*show_flash=*/false, color_);

  num_completed_flashes_++;

  if (num_completed_flashes_ >= kNumFlashesPerNotification) {
    num_completed_flashes_ = 0;
    return;
  }
  // Start the next flash.
  notification_timer_.Start(FROM_HERE, kNotificationTimerDelay, this,
                            &FlashScreenController::FlashOn);
}

void FlashScreenController::CancelTimer() {
  if (notification_timer_.IsRunning()) {
    notification_timer_.Stop();
  }
}

}  // namespace ash
