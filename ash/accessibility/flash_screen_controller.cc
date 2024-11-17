// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/flash_screen_controller.h"

#include "ash/color_enhancement/color_enhancement_controller.h"
#include "ash/shell.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

// Duration of the throb animation (on or off).
constexpr auto kNotificationTimerDelay = base::Milliseconds(300);

// The animation will be repeated twice.
constexpr int kNumFlashesPerNotification = 2;

}  // namespace

FlashScreenController::FlashScreenController() : throb_animation_{this} {
  notification_observer_.Observe(message_center::MessageCenter::Get());
  throb_animation_.SetThrobDuration(kNotificationTimerDelay);
}

FlashScreenController::~FlashScreenController() = default;

void FlashScreenController::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource display_source) {
  // Only flash when a popup is displayed (not the message center).
  if (display_source == message_center::DISPLAY_SOURCE_POPUP) {
    MaybeFlashOn(notification_id);
  }
}

void FlashScreenController::OnNotificationAdded(
    const std::string& notification_id) {
  MaybeFlashOn(notification_id);
}

void FlashScreenController::AnimationEnded(const gfx::Animation* animation) {
  // AnimationEnded is called at the end of each slide animation (up or down)
  // during the throb. Just turn the flash fully off when it's fully done.
  if (!throb_animation_.IsShowing()) {
    FlashOff();
  }
}

void FlashScreenController::AnimationProgressed(
    const gfx::Animation* animation) {
  const double percent = 1 - animation->GetCurrentValue();
  if (percent == 0) {
    FlashOff();
    return;
  }
  float r = SkColorGetR(color_);
  float g = SkColorGetG(color_);
  float b = SkColorGetB(color_);
  r = r + (255 - r) * percent;
  g = g + (255 - g) * percent;
  b = b + (255 - b) * percent;
  if (r > 255) {
    r = 255;
  }
  if (g > 255) {
    g = 255;
  }
  if (b > 255) {
    b = 255;
  }
  SkColor color = SkColorSetRGB(r, g, b);

  auto* color_enhancement_controller =
      Shell::Get()->color_enhancement_controller();
  color_enhancement_controller->FlashScreenForNotification(/*show_flash=*/true,
                                                           color);
}

void FlashScreenController::AnimationCanceled(const gfx::Animation* animation) {
  FlashOff();
}

void FlashScreenController::MaybeFlashOn(const std::string& notification_id) {
  auto* message_center = message_center::MessageCenter::Get();
  if (!message_center || message_center->IsQuietMode()) {
    // Do not flash when in quiet mode.
    return;
  }
  message_center::Notification* notification =
      message_center->FindNotificationById(notification_id);
  if (!notification || notification->silent()) {
    // Do not flash for silent notifications.
    return;
  }
  if (notification->group_parent()) {
    // Do not flash for new groupings of notifications (only for individual
    // notifications).
    return;
  }
  if (notification->priority() < message_center::DEFAULT_PRIORITY) {
    // Do not flash for low priority notifications, as no pop-up will
    // be shown.
    return;
  }
  FlashOn();
}

void FlashScreenController::PreviewFlash() {
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
  if (throb_animation_.is_animating()) {
    // Don't start a flash if already flashing.
    return;
  }

  throb_animation_.StartThrobbing(kNumFlashesPerNotification * 2);
}

void FlashScreenController::FlashOff() {
  // Turns off the flash.
  auto* color_enhancement_controller =
      Shell::Get()->color_enhancement_controller();
  color_enhancement_controller->FlashScreenForNotification(
      /*show_flash=*/false, color_);
}

}  // namespace ash
