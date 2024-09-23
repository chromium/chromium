// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_METRICS_UTILS_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_METRICS_UTILS_H_

#include <optional>

#include "ash/ash_export.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_popup_view.h"

namespace ash {

namespace metrics_utils {

// Keep in sync with enum in tools/metrics/histograms/enums.xml.
enum NotificationTypeDetailed : int {
  CHROME_APP_PRIORITY_MINUS_TWO = 0,
  CHROME_APP_PRIORITY_MINUS_TWO_REQUIRE_INTERACTION = 1,
  CHROME_APP_PRIORITY_MINUS_ONE = 2,
  CHROME_APP_PRIORITY_MINUS_ONE_REQUIRE_INTERACTION = 3,
  CHROME_APP_PRIORITY_ZERO = 4,
  CHROME_APP_PRIORITY_ZERO_REQUIRE_INTERACTION = 5,
  CHROME_APP_PRIORITY_ONE = 6,
  CHROME_APP_PRIORITY_ONE_REQUIRE_INTERACTION = 7,
  CHROME_APP_PRIORITY_TWO = 8,
  CHROME_APP_PRIORITY_TWO_REQUIRE_INTERACTION = 9,
  ARC_PRIORITY_MINUS_TWO = 10,
  ARC_PRIORITY_MINUS_TWO_PINNED = 11,
  ARC_PRIORITY_MINUS_ONE = 12,
  ARC_PRIORITY_MINUS_ONE_PINNED = 13,
  ARC_PRIORITY_ZERO = 14,
  ARC_PRIORITY_ZERO_PINNED = 15,
  ARC_PRIORITY_ONE = 16,
  ARC_PRIORITY_ONE_PINNED = 17,
  ARC_PRIORITY_TWO = 18,
  ARC_PRIORITY_TWO_PINNED = 19,
  WEB = 20,
  WEB_REQUIRE_INTERACTION = 21,
  CROS_PRIORITY_MINUS_TWO = 22,
  CROS_PRIORITY_MINUS_TWO_PINNED = 23,
  CROS_PRIORITY_MINUS_ONE = 24,
  CROS_PRIORITY_MINUS_ONE_PINNED = 25,
  CROS_PRIORITY_ZERO = 26,
  CROS_PRIORITY_ZERO_PINNED = 27,
  CROS_PRIORITY_ONE = 28,
  CROS_PRIORITY_ONE_PINNED = 29,
  CROS_PRIORITY_TWO = 30,
  CROS_PRIORITY_TWO_PINNED = 31,
  CROS_SYSTEM_PRIORITY = 32,
  CROS_SYSTEM_PRIORITY_PINNED = 33,
  CROS_SYSTEM_PRIORITY_WARNING = 34,
  CROS_SYSTEM_PRIORITY_WARNING_PINNED = 35,
  CROS_SYSTEM_PRIORITY_CRITICAL_WARNING = 36,
  CROS_SYSTEM_PRIORITY_CRITICAL_WARNING_PINNED = 37,
  OTHER = 38,
  PHONEHUB_PRIORITY_MINUS_TWO = 39,
  PHONEHUB_PRIORITY_MINUS_ONE = 40,
  PHONEHUB_PRIORITY_ZERO = 41,
  PHONEHUB_PRIORITY_ONE = 42,
  PHONEHUB_PRIORITY_TWO = 43,
  CHROME_APP_PRIORITY_OTHER = 44,
  CROSTINI = 45,
  kMaxValue = CROSTINI,
};

// These are the types of notification view that we are interested in recording
// metrics. A notification view might contain an image, a set of action buttons,
// and some of those buttons might enable inline reply. This enum covers all the
// possible variations for a notification view with image, action buttons and
// inline reply, and whether it is part of a group notification or not.
// Note to keep in sync with enum in tools/metrics/histograms/enums.xml.
enum class NotificationViewType {
  SIMPLE = 0,  // no image, action buttons and inline reply.
  GROUPED_SIMPLE = 1,
  HAS_IMAGE = 2,  // has image, no action buttons and inline reply.
  GROUPED_HAS_IMAGE = 3,
  HAS_ACTION = 4,  // has action buttons, no image and inline reply.
  GROUPED_HAS_ACTION = 5,
  HAS_INLINE_REPLY = 6,  // has inline reply, no image.
  GROUPED_HAS_INLINE_REPLY = 7,
  HAS_IMAGE_AND_ACTION = 8,  // has image and action button, no inline reply.
  GROUPED_HAS_IMAGE_AND_ACTION = 9,
  HAS_IMAGE_AND_INLINE_REPLY = 10,  // has image and inline reply.
  GROUPED_HAS_IMAGE_AND_INLINE_REPLY = 11,
  kMaxValue = GROUPED_HAS_IMAGE_AND_INLINE_REPLY,
};

// The actions that was performed after an expand button is clicked. These are
// used in histograms, do not remove/renumber entries. If you're adding to this
// enum with the intention that it will be logged, update the
// ExpandButtonClickAction token variant in enums.xml.
enum class ExpandButtonClickAction {
  EXPAND_INDIVIDUAL = 0,
  COLLAPSE_INDIVIDUAL = 1,
  EXPAND_GROUP = 2,
  COLLAPSE_GROUP = 3,
  kMaxValue = COLLAPSE_GROUP,
};

// The types of group notification. These are used in histograms, do not
// remove/renumber entries. If you're adding to this enum with the intention
// that it will be logged, update the ExpandButtonClickAction token variant in
// enums.xml.
enum class GroupNotificationType {
  GROUP_PARENT = 0,
  GROUP_CHILD = 1,
  kMaxValue = GROUP_CHILD,
};

// Returns the detailed notification type enum for a notification.
NotificationTypeDetailed GetNotificationType(
    const message_center::Notification& notification);

// Returns the detailed notification type enum for a notification id.
std::optional<NotificationTypeDetailed> GetNotificationType(
    const std::string& notification_id);

// Logs a Hover event on a notification.
void LogHover(const std::string& notification_id, bool is_popup);

// Logs a ClickedBody event.
void LogClickedBody(const std::string& notification_id, bool is_popup);

// Logs a ClickedActionButton event.
void LogClickedActionButton(const std::string& notification_id,
                            bool is_popup,
                            int button_index);

// Logs an InlineReplySent event.
ASH_EXPORT void LogInlineReplySent(const std::string& notification_id,
                                   bool is_popup);

// Logs a ExpireToTray event for a pop-up notification.
void LogPopupExpiredToTray(const std::string& notification_id);

// Logs a ClosedByUser event.
void LogClosedByUser(const std::string& notification_id,
                     bool is_swipe,
                     bool is_popup);

// Logs a SettingsShown event.
void LogSettingsShown(const std::string& notification_id,
                      bool is_slide_controls,
                      bool is_popup);

// Logs a Snoozed event.
void LogSnoozed(const std::string& notification_id,
                bool is_slide_controls,
                bool is_popup);

// Logs a popup Shown event.
void LogPopupShown(const std::string& notification_id);

// Logs a popup Closed event.
void LogPopupClosed(message_center::MessagePopupView* popup);

// Logs a tray ClosedByClearAll event.
void LogClosedByClearAll(const std::string& notification_id);

// Logs a notification added event.
void LogNotificationAdded(const std::string& notification_id);

// Logs a system notification added event.
void LogSystemNotificationAdded(const std::string& notification_id);

// Logs the count of notifications displayed during the first minute after a
// user logs in.
void LogNotificationsShownInFirstMinute(int notifications_count);

// Logs the number of notifications contained in a group.
void LogCountOfNotificationsInOneGroup(int notification_count);

// Logs the action that was performed after an expand button is clicked,
void LogExpandButtonClickAction(ExpandButtonClickAction action);

// Logs the type of group notification added to the system.
void LogGroupNotificationAddedType(GroupNotificationType type);

// Logs when an ongoing process was shown without having provided a
// `vector_small_image`.
void LogOngoingProcessShownWithoutIcon(NotificationCatalogName catalog_name);

// Logs when an ongoing process was shown without having provided a `title`.
void LogOngoingProcessShownWithoutTitle(NotificationCatalogName catalog_name);

}  // namespace metrics_utils

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_METRICS_UTILS_H_
