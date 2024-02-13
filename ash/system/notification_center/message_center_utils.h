// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_UTILS_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_UTILS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "ui/gfx/animation/tween.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace message_center {
class NotificationViewController;
}

namespace views {
class View;
}

namespace ash {

class NotificationGroupingController;

namespace message_center_utils {

// Return a hash string derived from `notifier_id` data. This is appended to a
// notification's `id` to create a unique identifier for a grouped notification.
std::string ASH_EXPORT
GenerateGroupParentNotificationIdSuffix(message_center::NotifierId notifier_id);

// Comparator function for sorting the notifications in the order that they
// should be displayed. Currently the ordering rule is very simple (subject to
// change):
//     1. All pinned notifications are displayed first.
//     2. Otherwise, display in order of most recent timestamp.
bool CompareNotifications(message_center::Notification* n1,
                          message_center::Notification* n2);

// Returns a vector of notifications that should have their own message
// view sorted for display, using CompareNotifications() above for the sorting
// order.
std::vector<message_center::Notification*> ASH_EXPORT
GetSortedNotificationsWithOwnView();

// Returns total notifications count, with a filter to not count some of them
// These notifications such as camera, media controls, etc. don't need an
// indicator in status area since they already have a dedicated tray item, and
// grouped notifications only need to be counted as one.
size_t ASH_EXPORT GetNotificationCount();

// Returns true if there are any notifications hidden because we're on the
// lockscreen. Should be only called if the screen is locked.
bool AreNotificationsHiddenOnLockscreen();

// Get the notification view controller associated to a certain display.
message_center::NotificationViewController*
GetActiveNotificationViewControllerForDisplay(int64_t display_id);

// Get the currently active notification view controller for the provided
// `notification_view`. Each screen has it's own `MessagePopupCollection` and
// `UnifiedMessageListView`.
message_center::NotificationViewController*
GetActiveNotificationViewControllerForNotificationView(
    views::View* notification_view);

// Gets the grouping controller for the provided notification view. Each display
// has it's own `NotificationGroupingController` so we need to look up the
// display for the provide view first.
NotificationGroupingController* ASH_EXPORT
GetGroupingControllerForNotificationView(views::View* notification_view);

// Utils for animation within a notification view.

// Initializes the layer for the specified `view` for animations.
void ASH_EXPORT InitLayerForAnimations(views::View* view);

// Fade in animation using AnimationBuilder.
void ASH_EXPORT
FadeInView(views::View* view,
           int delay_in_ms,
           int duration_in_ms,
           gfx::Tween::Type tween_type = gfx::Tween::LINEAR,
           const std::string& animation_histogram_name = std::string());

// Fade out animation using AnimationBuilder.
void ASH_EXPORT
FadeOutView(views::View* view,
            base::OnceClosure on_animation_ended,
            int delay_in_ms,
            int duration_in_ms,
            gfx::Tween::Type tween_type = gfx::Tween::LINEAR,
            const std::string& animation_histogram_name = std::string());

// Slide out animation using AnimationBuilder.
void SlideOutView(views::View* view,
                  base::OnceClosure on_animation_ended,
                  base::OnceClosure on_animation_aborted,
                  int delay_in_ms,
                  int duration_in_ms,
                  gfx::Tween::Type tween_type = gfx::Tween::LINEAR,
                  const std::string& animation_histogram_name = std::string());

// Returns the resized image if the binary size of `input_image` is greater than
// `size_limit_in_byte`. Otherwise, returns `std::nullopt`.
[[nodiscard]] ASH_EXPORT std::optional<gfx::ImageSkia>
ResizeImageIfExceedSizeLimit(const gfx::ImageSkia& input_image,
                             size_t size_limit_in_byte);

// Check if the view can be casted to `AshNotificationView`.
// TODO(b/308814203): remove this function after cleaning the static_cast
// checks for casting `AshNotificationView*`.
bool IsAshNotificationView(views::View* sender);

// Check if the notification is Ash notification.
// TODO(b/308814203): remove this function after cleaning the static_cast
// checks for casting `AshNotificationView*`.
bool IsAshNotification(const message_center::Notification* notification);

}  // namespace message_center_utils
}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_UTILS_H_
