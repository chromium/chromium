// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_TYPES_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_TYPES_H_

#include <map>
#include <optional>
#include <string>

namespace notifications {

// The type of a list of clients using the notification scheduler system. Used
// in metrics, need to sync with histogram suffix
// NotificationSchedulerClientType in histograms.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.notifications.scheduler)
enum class SchedulerClientType {
  // Test only values.
  kTest1 = -1,
  kTest2 = -2,
  kTest3 = -3,

  // Default value of client type.
  kUnknown = 0,
  // Client used in chrome://notifications-internals for debugging.
  kWebUI = 1,
  // Chrome update notification.
  kChromeUpdate = 2,
  // Offline prefetch notification. (Deprecated)
  kPrefetch = 3,
  // Reading list weekly notification.
  kReadingList = 4,
  // Feature guide specific notifications. (Deprecated)
  kDeprecatedFeatureGuide = 5,
  kMaxValue = kDeprecatedFeatureGuide
};

// The type of user feedback from a displayed notification.
enum class UserFeedback {
  // No user feedback yet.
  kNoFeedback = 0,
  // The user taps the helpful button. By default, generates positive
  // ImpressionResult and may increase the notification display frequency.
  kHelpful = 1,
  // The user taps the unhelpful button. By default, generates negative
  // ImpressionResult and may decrease the notification display frequency.
  kNotHelpful = 2,
  // The user clicks the notification. By default, generates positive
  // ImpressionResult and may increase the notification display frequency.
  kClick = 3,
  // The user dismisses notification. Only consecutive dismisses generates
  // ImpressionResult.
  // By default, generates neutral impression result and will not change the
  // notification display frequency.
  kDismiss = 4,
  // The user has no interaction with the notification for a while.
  kIgnore = 5,
  kMaxValue = kIgnore
};

// The user impression result of a particular notification, which may impact the
// notification display frenquency.
enum class ImpressionResult {
  // Invalid user impression.
  kInvalid = 0,
  // Positive user impression that the user may like the notification.
  kPositive = 1,
  // Negative user impression that the user may dislike the notification.
  kNegative = 2,
  // The feedback is neutral to the user.
  kNeutral = 3,
  kMaxValue = kNeutral
};

// Defines user actions type. Used in metrics, can only insert enum values, need
// to sync with histogram enum NotificationSchedulerUserActionType in
// enums.xml. A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.notifications.scheduler)
enum class UserActionType {
  // The user clicks on the notification body.
  kClick = 0,
  // The user clicks on the notification button.
  kButtonClick = 1,
  // The user dismisses the notification.
  kDismiss = 2,
  kMaxValue = kDismiss
};

// Categorizes type of notification buttons. Different type of button clicks
// may result in change of notification shown frequency.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.notifications.scheduler)
enum class ActionButtonType {
  // The action button is not categorized.
  kUnknownAction = 0,

  // Helpful button indicates the user likes to interact with the notification.
  kHelpful = 1,

  // Unhelpful button indicates dislike of the notification.
  kUnhelpful = 2,
};

// Information about button clicks.
struct ButtonClickInfo {
  // Unique id of the button.
  std::string button_id;

  // Associate impression type for the button.
  ActionButtonType type = ActionButtonType::kUnknownAction;
};

// Contains data associated with user actions.
struct UserActionData {
  UserActionData(SchedulerClientType client_type,
                 UserActionType action_type,
                 const std::string& guid);
  UserActionData(const UserActionData& other);
  ~UserActionData();
  bool operator==(const UserActionData& other) const;

  // The type of the client that sent the notification.
  const SchedulerClientType client_type;

  // The user action type.
  const UserActionType action_type;

  // The guid of the notification.
  const std::string guid;

  // The client defined custom data.
  std::map<std::string, std::string> custom_data;

  // The button click info, only available when the user clicked a button.
  std::optional<ButtonClickInfo> button_click_info;
};

// Categorizes type of notification icons.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.notifications.scheduler)
enum class IconType {
  kUnknownType = 0,
  kSmallIcon = 1,
  kLargeIcon = 2,
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULER_TYPES_H_
