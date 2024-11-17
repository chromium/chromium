// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_FLASH_SCREEN_CONTROLLER_H_
#define ASH_ACCESSIBILITY_FLASH_SCREEN_CONTROLLER_H_

#include <string>

#include "ash/constants/ash_constants.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"

namespace ash {

// Class to control the feature which flashes the screen when a notification
// is shown.
class FlashScreenController : public message_center::MessageCenterObserver,
                              public gfx::AnimationDelegate {
 public:
  FlashScreenController();
  FlashScreenController(const FlashScreenController&) = delete;
  FlashScreenController& operator=(const FlashScreenController&) = delete;
  ~FlashScreenController() override;

  // MessageCenterObserver:
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource display_source) override;
  void OnNotificationAdded(const std::string& notification_id) override;

  // AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Runs the flash notification without any notification needing to be
  // displayed.
  void PreviewFlash();

  // No need to stop an ongoing flash if one is happening, the duration is too
  // short.
  void set_enabled(bool enabled) { enabled_ = enabled; }

  void set_color(const SkColor& color) { color_ = color; }

  gfx::ThrobAnimation* GetAnimationForTesting() { return &throb_animation_; }

 private:
  void MaybeFlashOn(const std::string& notification_id);
  void FlashOn();
  void FlashOff();

  bool enabled_ = false;
  SkColor color_ = kDefaultFlashNotificationsColor;

  // A timer that ends the flash screen color.
  base::RetainingOneShotTimer notification_timer_;

  gfx::ThrobAnimation throb_animation_;

  // How many flashes have elapsed for this timer.
  int num_completed_flashes_ = 0;

  base::ScopedObservation<message_center::MessageCenter, MessageCenterObserver>
      notification_observer_{this};
};
}  // namespace ash

#endif  // ASH_ACCESSIBILITY_FLASH_SCREEN_CONTROLLER_H_
