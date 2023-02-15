// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_ANIMATION_CONTROLLER_H_
#define ASH_SYSTEM_STATUS_AREA_ANIMATION_CONTROLLER_H_

#include <list>

#include "ash/ash_export.h"
#include "base/functional/callback_helpers.h"
#include "ui/views/view_observer.h"

namespace ash {

class NotificationCenterTray;

// This class controls the animation sequence that runs when the notification
// center tray's visibility changes.
class ASH_EXPORT StatusAreaAnimationController : public views::ViewObserver {
 public:
  explicit StatusAreaAnimationController(
      NotificationCenterTray* notification_center_tray);
  StatusAreaAnimationController(const StatusAreaAnimationController&) = delete;
  StatusAreaAnimationController& operator=(
      const StatusAreaAnimationController&) = delete;
  ~StatusAreaAnimationController() override;

 private:
  // Starts running the visibility animation sequence. This will be the "show"
  // animation sequence if `visible` is true, otherwise it will be the "hide"
  // sequence.
  void PerformAnimation(bool visible);

  // Disables animations for visibility changes of the notification center
  // tray's `TrayItemView`s. The animations will be re-enabled when the higher-
  // level "show" animation sequence finishes/aborts.
  void DisableNotificationCenterTrayItemAnimations();

  // Re-enables the notification center tray's `TrayItemView`'s visibility
  // animations.
  void EnableNotificationCenterTrayItemAnimations();

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

  // A `base::ScopedClosureRunner` that, when run, re-enables default visibility
  // animations for `NotificationCenterTray`. Note that this should not be run
  // until this `StatusAreaAnimationController` is being destroyed, because the
  // whole point of this class is to handle a custom visibility animation for
  // `NotificationCenterTray`.
  std::unique_ptr<base::ScopedClosureRunner>
      notification_center_tray_default_animation_enabler_;

  // A list of `base::ScopedClosureRunner`s that, when run, re-enable visibility
  // animations for `NotificationCenterTray`'s `TrayItemView`s whose animations
  // are currently disabled.
  std::list<base::ScopedClosureRunner>
      notification_center_tray_item_animation_enablers_;
  NotificationCenterTray* notification_center_tray_;

  base::WeakPtrFactory<StatusAreaAnimationController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_ANIMATION_CONTROLLER_H_
