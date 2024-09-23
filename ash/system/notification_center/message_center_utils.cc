// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/message_center_utils.h"

#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_controller.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/notification_grouping_controller.h"
#include "ash/system/notification_center/session_state_notification_blocker.h"
#include "ash/system/status_area_widget.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/message_center/message_center.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

void ReportAnimationSmoothness(const std::string& animation_histogram_name,
                               int smoothness) {
  // Record animation smoothness if `animation_histogram_name` is given.
  if (!animation_histogram_name.empty()) {
    base::UmaHistogramPercentage(animation_histogram_name, smoothness);
  }
}

}  // namespace

namespace ash::message_center_utils {

std::string GenerateGroupParentNotificationIdSuffix(
    message_center::NotifierId notifier_id) {
  switch (notifier_id.type) {
    case message_center::NotifierType::WEB_PAGE:
      return base::SHA1HashString(notifier_id.url.spec() +
                                  notifier_id.web_app_id.value_or(""));
    case message_center::NotifierType::ARC_APPLICATION:
      return base::SHA1HashString(notifier_id.id +
                                  notifier_id.group_key.value_or(""));
    case message_center::NotifierType::SYSTEM_COMPONENT:
      if (notifier_id.id == ash::kPrivacyIndicatorsNotifierId) {
        return base::SHA1HashString(notifier_id.id);
      }
      ABSL_FALLTHROUGH_INTENDED;
    case message_center::NotifierType::APPLICATION:
    case message_center::NotifierType::CROSTINI_APPLICATION:
    case message_center::NotifierType::PHONE_HUB:
      NOTREACHED();
  }
}

bool CompareNotifications(message_center::Notification* n1,
                          message_center::Notification* n2) {
  if (n1->pinned() && !n2->pinned()) {
    return true;
  }
  if (!n1->pinned() && n2->pinned()) {
    return false;
  }
  return message_center::CompareTimestampSerial()(n1, n2);
}

std::vector<message_center::Notification*> GetSortedNotificationsWithOwnView() {
  std::vector<message_center::Notification*> sorted_notifications;
  base::ranges::copy_if(
      message_center::MessageCenter::Get()->GetVisibleNotifications(),
      std::back_inserter(sorted_notifications),
      [](message_center::Notification* notification) {
        return !notification->group_child();
      });
  std::sort(sorted_notifications.begin(), sorted_notifications.end(),
            CompareNotifications);
  return sorted_notifications;
}

size_t GetNotificationCount() {
  // We need to ignore the `session_state_notification_blocker` when getting the
  // notification count on the lock screen. This is because we want the counter
  // to show the total number of available notifications including notifications
  // that are hidden by the blocker.
  const message_center::NotificationBlocker* blocker_to_ignore =
      Shell::Get()->session_controller()->IsScreenLocked()
          ? Shell::Get()
                ->message_center_controller()
                ->session_state_notification_blocker()
          : nullptr;

  return base::ranges::count_if(
      message_center::MessageCenter::Get()
          ->GetVisibleNotificationsWithoutBlocker(blocker_to_ignore),
      [](message_center::Notification* notification) {
        const std::string& notifier = notification->notifier_id().id;

        // Don't count these notifications since we have
        // `PrivacyIndicatorsTrayItemView` or `CameraMicTrayItemView` to show
        // indicators on the systray.
        if (notifier == kPrivacyIndicatorsNotifierId ||
            notifier == kVmCameraMicNotifierId) {
          return false;
        }

        // The lockscreen notification is used to signify that there are
        // notifications hidden. It should not effect the number of
        // notifications.
        if (notifier == kLockScreenNotifierId) {
          return false;
        }
        // Don't count group child notifications since they're contained in a
        // single parent view.
        if (notification->group_child()) {
          return false;
        }

        return true;
      });
}

bool AreNotificationsHiddenOnLockscreen() {
  DCHECK(Shell::Get()->session_controller()->IsScreenLocked());

  // Return true if the `session_state_notification_blocker` is hiding any
  // notifications.
  auto* message_center = message_center::MessageCenter::Get();
  if (message_center->GetVisibleNotifications().size() !=
      message_center
          ->GetVisibleNotificationsWithoutBlocker(
              Shell::Get()
                  ->message_center_controller()
                  ->session_state_notification_blocker())
          .size()) {
    return true;
  }

  return false;
}

message_center::NotificationViewController*
GetActiveNotificationViewControllerForDisplay(int64_t display_id) {
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display_id);
  // Can be null in tests.
  if (!root_window_controller ||
      !root_window_controller->GetStatusAreaWidget()) {
    return nullptr;
  }

  return root_window_controller->GetStatusAreaWidget()
      ->notification_center_tray()
      ->notification_grouping_controller()
      ->GetActiveNotificationViewController();
}

message_center::NotificationViewController*
GetActiveNotificationViewControllerForNotificationView(
    views::View* notification_view) {
  aura::Window* window = notification_view->GetWidget()->GetNativeWindow();
  auto display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();

  return GetActiveNotificationViewControllerForDisplay(display_id);
}

NotificationGroupingController* GetGroupingControllerForNotificationView(
    views::View* notification_view) {
  aura::Window* window = notification_view->GetWidget()->GetNativeWindow();
  auto display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();

  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display_id);
  // Can be null in tests.
  if (!root_window_controller ||
      !root_window_controller->GetStatusAreaWidget()) {
    return nullptr;
  }

  return root_window_controller->GetStatusAreaWidget()
      ->notification_center_tray()
      ->notification_grouping_controller();
}

void InitLayerForAnimations(views::View* view) {
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
}

void FadeInView(views::View* view,
                int delay_in_ms,
                int duration_in_ms,
                gfx::Tween::Type tween_type,
                const std::string& animation_histogram_name) {
  // If we are in testing with animation (non zero duration), we shouldn't have
  // delays so that we can properly track when animation is completed in test.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION) {
    delay_in_ms = 0;
  }

  // The view must have a layer to perform animation.
  DCHECK(view->layer());

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating(
          &ReportAnimationSmoothness, animation_histogram_name)));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(view, 0.0f)
      .At(base::Milliseconds(delay_in_ms))
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetOpacity(view, 1.0f, tween_type);
}

void FadeOutView(views::View* view,
                 base::OnceClosure on_animation_ended,
                 int delay_in_ms,
                 int duration_in_ms,
                 gfx::Tween::Type tween_type,
                 const std::string& animation_histogram_name) {
  // If we are in testing with animation (non zero duration), we shouldn't have
  // delays so that we can properly track when animation is completed in test.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION) {
    delay_in_ms = 0;
  }

  std::pair<base::OnceClosure, base::OnceClosure> split =
      base::SplitOnceCallback(std::move(on_animation_ended));

  // The view must have a layer to perform animation.
  DCHECK(view->layer());

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating(
          &ReportAnimationSmoothness, animation_histogram_name)));

  view->SetVisible(true);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .At(base::Milliseconds(delay_in_ms))
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetVisibility(view, false)
      .SetOpacity(view, 0.0f, tween_type);
}

void SlideOutView(views::View* view,
                  base::OnceClosure on_animation_ended,
                  base::OnceClosure on_animation_aborted,
                  int delay_in_ms,
                  int duration_in_ms,
                  gfx::Tween::Type tween_type,
                  const std::string& animation_histogram_name) {
  // If we are in testing with animation (non zero duration), we shouldn't have
  // delays so that we can properly track when animation is completed in test.
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION) {
    delay_in_ms = 0;
  }

  // The view must have a layer to perform animation.
  DCHECK(view->layer());

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating(
          &ReportAnimationSmoothness, animation_histogram_name)));

  gfx::Transform transform;
  transform.Translate(gfx::Vector2dF(view->bounds().width(), 0));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(on_animation_ended))
      .OnAborted(std::move(on_animation_aborted))
      .Once()
      .At(base::Milliseconds(delay_in_ms))
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetTransform(view->layer(), transform);
}

std::optional<gfx::ImageSkia> ResizeImageIfExceedSizeLimit(
    const gfx::ImageSkia& input_image,
    size_t size_limit_in_byte) {
  const size_t image_size_in_bytes = input_image.bitmap()->computeByteSize();
  if (image_size_in_bytes <= size_limit_in_byte) {
    return std::nullopt;
  }

  // Calculate the image size after resize.
  gfx::SizeF resized_size(input_image.size());
  const float multiple =
      image_size_in_bytes / static_cast<float>(size_limit_in_byte);
  resized_size.Scale(1 / std::sqrt(multiple));

  return gfx::ImageSkiaOperations::CreateResizedImage(
      input_image, skia::ImageOperations::RESIZE_BEST,
      gfx::ToFlooredSize(resized_size));
}

bool IsAshNotificationView(views::View* sender) {
  auto* message_view = static_cast<message_center::MessageView*>(sender);
  std::string notification_id = message_view->notification_id();

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);

  return IsAshNotification(notification);
}

bool IsAshNotification(const message_center::Notification* notification) {
  if (!notification ||
      (notification->type() == message_center::NOTIFICATION_TYPE_CUSTOM &&
       notification->notifier_id().type ==
           message_center::NotifierType::ARC_APPLICATION)) {
    return false;
  }
  return true;
}

}  // namespace ash::message_center_utils
