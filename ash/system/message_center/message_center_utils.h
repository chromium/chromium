// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UTILS_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UTILS_H_

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "ui/gfx/animation/tween.h"
#include "ui/message_center/public/cpp/notification.h"

namespace views {
class View;
}

namespace ash {

namespace message_center_utils {

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
std::vector<message_center::Notification*> GetSortedNotificationsWithOwnView();

// Returns total notifications count, with a filter to not count some of them
// These notifications such as camera, media controls, etc. don't need an
// indicator in status area since they already have a dedicated tray item, and
// grouped notifications only need to be counted as one.
size_t ASH_EXPORT GetNotificationCount();

// Utils for animation within a notification view.

// Initializes the layer for the specified `view` for animations.
void InitLayerForAnimations(views::View* view);

// Fade in animation using AnimationBuilder.
void FadeInView(views::View* view,
                int delay_in_ms,
                int duration_in_ms,
                gfx::Tween::Type tween_type = gfx::Tween::LINEAR,
                const std::string& animation_histogram_name = std::string());

// Fade out animation using AnimationBuilder.
void FadeOutView(views::View* view,
                 base::OnceClosure on_animation_ended,
                 int delay_in_ms,
                 int duration_in_ms,
                 gfx::Tween::Type tween_type = gfx::Tween::LINEAR,
                 const std::string& animation_histogram_name = std::string());

}  // namespace message_center_utils

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_UTILS_H_
