// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_animation_controller.h"

#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/tray/tray_container.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/animation_builder.h"

namespace ash {

StatusAreaAnimationController::StatusAreaAnimationController(
    NotificationCenterTray* notification_center_tray)
    : notification_center_tray_(notification_center_tray) {
  if (!notification_center_tray_) {
    return;
  }

  notification_center_tray_->AddTrayBackgroundViewObserver(this);
  notification_center_tray_->AddNotificationCenterTrayObserver(this);
  notification_center_tray_default_animation_enabler_ =
      std::make_unique<base::ScopedClosureRunner>(
          notification_center_tray->SetUseCustomVisibilityAnimations());
  notification_center_tray_item_animation_enablers_ =
      std::list<base::ScopedClosureRunner>();
}

StatusAreaAnimationController::~StatusAreaAnimationController() {
  if (notification_center_tray_) {
    notification_center_tray_->RemoveNotificationCenterTrayObserver(this);
    notification_center_tray_->RemoveTrayBackgroundViewObserver(this);
  }
}

void StatusAreaAnimationController::OnVisiblePreferredChanged(
    bool visible_preferred) {
  PerformAnimation(visible_preferred);
}

void StatusAreaAnimationController::OnAllTrayItemsAdded() {
  // `NotificationCenterTray`'s `TrayItemView`s need to have their animations
  // disabled ahead of the first time the `NotificationCenterTray` becomes
  // visible. This is the right time to disable those animations because it is
  // after all the `TrayItemView`s have been added to the tray but before the
  // tray has had a chance to update its visibility from the default non-visible
  // state.
  DisableNotificationCenterTrayItemAnimations();
}

void StatusAreaAnimationController::
    DisableNotificationCenterTrayItemAnimations() {
  for (views::View* tray_item :
       notification_center_tray_->tray_container()->children()) {
    notification_center_tray_item_animation_enablers_.push_back(
        static_cast<TrayItemView*>(tray_item)->DisableAnimation());
  }
}

void StatusAreaAnimationController::
    EnableNotificationCenterTrayItemAnimations() {
  notification_center_tray_item_animation_enablers_.clear();
}

void StatusAreaAnimationController::PerformAnimation(bool visible) {
  if (visible) {
    notification_center_tray_->layer()->SetVisible(true);
    views::AnimationBuilder()
        .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                   IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnAborted(base::BindOnce(
            [](base::WeakPtr<StatusAreaAnimationController> ptr) {
              if (!ptr || !ptr->notification_center_tray_) {
                return;
              }
              ptr->notification_center_tray_->OnAnimationAborted();

              // Don't enable notification center tray item animations if this
              // show animation was interrupted by a hide animation.
              if (!ptr->notification_center_tray_->visible_preferred()) {
                return;
              }
              ptr->EnableNotificationCenterTrayItemAnimations();
            },
            weak_factory_.GetWeakPtr()))
        .OnEnded(base::BindOnce(
            [](base::WeakPtr<StatusAreaAnimationController> ptr) {
              if (!ptr || !ptr->notification_center_tray_) {
                return;
              }
              ptr->notification_center_tray_->OnAnimationEnded();
              ptr->EnableNotificationCenterTrayItemAnimations();
            },
            weak_factory_.GetWeakPtr()))
        .Once()
        .Offset(base::Milliseconds(50))
        .SetDuration(base::Milliseconds(150))
        .SetOpacity(notification_center_tray_, 1, gfx::Tween::LINEAR);
  } else {
    DisableNotificationCenterTrayItemAnimations();
    views::AnimationBuilder()
        .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                   IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .OnScheduled(base::BindOnce(
            [](base::WeakPtr<StatusAreaAnimationController> ptr) {
              if (!ptr) {
                return;
              }
              ptr->is_hide_animation_scheduled_ = true;
            },
            weak_factory_.GetWeakPtr()))
        .OnStarted(base::BindOnce(
            [](base::WeakPtr<StatusAreaAnimationController> ptr) {
              if (!ptr || !ptr->notification_center_tray_) {
                return;
              }
              ptr->notification_center_tray_->OnHideAnimationStarted();
            },
            weak_factory_.GetWeakPtr()))
        .OnAborted(base::BindOnce(
            [](base::WeakPtr<StatusAreaAnimationController> ptr) {
              if (!ptr || !ptr->notification_center_tray_) {
                return;
              }
              ptr->is_hide_animation_scheduled_ = false;
              ptr->notification_center_tray_->OnAnimationAborted();
              ptr->ImmediatelyUpdateTrayItemVisibilities();
            },
            weak_factory_.GetWeakPtr()))
        .OnEnded(base::BindOnce(
            [](base::WeakPtr<StatusAreaAnimationController> ptr) {
              if (!ptr || !ptr->notification_center_tray_) {
                return;
              }
              ptr->is_hide_animation_scheduled_ = false;
              ptr->notification_center_tray_->OnAnimationEnded();
              ptr->ImmediatelyUpdateTrayItemVisibilities();
            },
            weak_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(base::Milliseconds(150))
        .SetOpacity(notification_center_tray_, 0, gfx::Tween::LINEAR)
        .SetVisibility(notification_center_tray_, false);
  }
}

void StatusAreaAnimationController::ImmediatelyUpdateTrayItemVisibilities() {
  for (views::View* tray_item :
       notification_center_tray_->tray_container()->children()) {
    static_cast<TrayItemView*>(tray_item)->ImmediatelyUpdateVisibility();
  }
}

}  // namespace ash
