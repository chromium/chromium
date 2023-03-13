// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status_area_animation_controller.h"

#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/unified/notification_counter_view.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/animation_builder.h"

namespace ash {

StatusAreaAnimationController::StatusAreaAnimationController(
    NotificationCenterTray* notification_center_tray)
    : notification_center_tray_(notification_center_tray) {
  if (!notification_center_tray_) {
    return;
  }

  notification_center_tray_->AddObserver(this);
  notification_center_tray_default_animation_enabler_ =
      std::make_unique<base::ScopedClosureRunner>(
          notification_center_tray->SetUseCustomVisibilityAnimations());
  notification_center_tray_item_animation_enablers_ =
      std::list<base::ScopedClosureRunner>();
  DisableNotificationCenterTrayItemAnimations();
}

StatusAreaAnimationController::~StatusAreaAnimationController() {
  if (notification_center_tray_) {
    notification_center_tray_->RemoveObserver(this);
  }
}

void StatusAreaAnimationController::OnVisiblePreferredChanged(
    bool visible_preferred) {
  PerformAnimation(visible_preferred);
}

void StatusAreaAnimationController::
    DisableNotificationCenterTrayItemAnimations() {
  auto* notification_icons_controller =
      notification_center_tray_->notification_icons_controller();
  for (auto* tray_item : notification_icons_controller->tray_items()) {
    notification_center_tray_item_animation_enablers_.push_back(
        tray_item->DisableAnimation());
  }
  // Don't forget about the `TrayItemView`s that are still children of
  // `NotificationCenterTray` even though they're not part of
  // `notification_icons_controller->tray_items()`.
  notification_center_tray_item_animation_enablers_.push_back(
      notification_icons_controller->notification_counter_view()
          ->DisableAnimation());
  notification_center_tray_item_animation_enablers_.push_back(
      notification_icons_controller->quiet_mode_view()->DisableAnimation());
}

void StatusAreaAnimationController::
    EnableNotificationCenterTrayItemAnimations() {
  notification_center_tray_item_animation_enablers_.clear();
}

void StatusAreaAnimationController::PerformAnimation(bool visible) {
  if (visible) {
    notification_center_tray_->layer()->SetVisible(true);
    notification_center_tray_->layer()->SetTransform(gfx::Transform());
    views::AnimationBuilder()
        .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                   IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnAborted(base::BindOnce(
            [](base::WeakPtr<StatusAreaAnimationController> ptr) {
              if (ptr) {
                ptr->EnableNotificationCenterTrayItemAnimations();
              }
            },
            weak_factory_.GetWeakPtr()))
        .OnEnded(base::BindOnce(
            [](base::WeakPtr<StatusAreaAnimationController> ptr) {
              if (ptr) {
                ptr->EnableNotificationCenterTrayItemAnimations();
              }
            },
            weak_factory_.GetWeakPtr()))
        .Once()
        .Offset(base::Milliseconds(50))
        .SetDuration(base::Milliseconds(150))
        .SetOpacity(notification_center_tray_, 1, gfx::Tween::LINEAR);
  } else {
    DisableNotificationCenterTrayItemAnimations();
    // TODO(b/252887047): Replace default hide animation with new hide
    // animation.
  }
}

}  // namespace ash
