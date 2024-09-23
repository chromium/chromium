// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/metrics_utils.h"

#include <optional>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/message_view.h"

namespace {

// Used in histogram names that are persisted to metric logs.
std::string GetNotifierFrameworkNotificationHistogramBase(bool pinned) {
  if (pinned)
    return "Ash.NotifierFramework.PinnedSystemNotification";
  return "Ash.NotifierFramework.SystemNotification";
}

// Used in histogram names that are persisted to metric logs.
std::string GetNotifierFrameworkPopupDismissedTimeRange(
    const base::TimeDelta& time) {
  if (time <= base::Seconds(1))
    return "Within1s";
  if (time <= base::Seconds(3))
    return "Within3s";
  if (time <= base::Seconds(7))
    return "Within7s";
  return "After7s";
}

bool ValidCatalogName(const message_center::NotifierId& notifier_id) {
  if (notifier_id.type != message_center::NotifierType::SYSTEM_COMPONENT)
    return false;

  if (notifier_id.catalog_name == ash::NotificationCatalogName::kNone) {
    return false;
  }

  return true;
}

}  // namespace

namespace ash {
namespace metrics_utils {

NotificationTypeDetailed GetNotificationTypeForChromeApp(
    const message_center::Notification& notification) {
  int priority = notification.rich_notification_data().priority;
  bool require_interaction =
      notification.rich_notification_data().never_timeout;

  if (require_interaction) {
    switch (priority) {
      case -2:
        return CHROME_APP_PRIORITY_MINUS_TWO_REQUIRE_INTERACTION;
      case -1:
        return CHROME_APP_PRIORITY_MINUS_ONE_REQUIRE_INTERACTION;
      case 0:
        return CHROME_APP_PRIORITY_ZERO_REQUIRE_INTERACTION;
      case 1:
        return CHROME_APP_PRIORITY_ONE_REQUIRE_INTERACTION;
      case 2:
        return CHROME_APP_PRIORITY_TWO_REQUIRE_INTERACTION;
      default:
        // Note that after launch/4288967, we will ignore require interaction in
        // chromeOS. Thus, we don't need to record require interaction metrics
        // in CrOS from now on.
        return CHROME_APP_PRIORITY_OTHER;
    }
  } else {
    switch (priority) {
      case -2:
        return CHROME_APP_PRIORITY_MINUS_TWO;
      case -1:
        return CHROME_APP_PRIORITY_MINUS_ONE;
      case 0:
        return CHROME_APP_PRIORITY_ZERO;
      case 1:
        return CHROME_APP_PRIORITY_ONE;
      case 2:
        return CHROME_APP_PRIORITY_TWO;
      default:
        return CHROME_APP_PRIORITY_OTHER;
    }
  }
}

NotificationTypeDetailed GetNotificationTypeForArc(
    const message_center::Notification& notification) {
  int priority = notification.rich_notification_data().priority;
  bool pinned = notification.rich_notification_data().pinned;

  if (pinned) {
    switch (priority) {
      case -2:
        return ARC_PRIORITY_MINUS_TWO_PINNED;
      case -1:
        return ARC_PRIORITY_MINUS_ONE_PINNED;
      case 0:
        return ARC_PRIORITY_ZERO_PINNED;
      case 1:
        return ARC_PRIORITY_ONE_PINNED;
      case 2:
        return ARC_PRIORITY_TWO_PINNED;
      default:
        NOTREACHED();
    }
  } else {
    switch (priority) {
      case -2:
        return ARC_PRIORITY_MINUS_TWO;
      case -1:
        return ARC_PRIORITY_MINUS_ONE;
      case 0:
        return ARC_PRIORITY_ZERO;
      case 1:
        return ARC_PRIORITY_ONE;
      case 2:
        return ARC_PRIORITY_TWO;
      default:
        NOTREACHED();
    }
  }
}

NotificationTypeDetailed GetNotificationTypeForCrosSystemPriority(
    const message_center::Notification& notification) {
  // The warning level is not stored in the notification data, so we need to
  // infer it from the accent color.
  std::optional<SkColor> accent_color =
      notification.rich_notification_data().accent_color;
  std::optional<ui::ColorId> accent_color_id =
      notification.rich_notification_data().accent_color_id;
  message_center::SystemNotificationWarningLevel warning_level =
      message_center::SystemNotificationWarningLevel::NORMAL;
  if (accent_color.has_value()) {
    if (accent_color.value() == kSystemNotificationColorWarning) {
      warning_level = message_center::SystemNotificationWarningLevel::WARNING;
    } else if (accent_color.value() ==
               kSystemNotificationColorCriticalWarning) {
      warning_level =
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
    }
  } else if (accent_color_id.has_value()) {
    if (accent_color_id.value() == cros_tokens::kCrosSysWarning) {
      warning_level = message_center::SystemNotificationWarningLevel::WARNING;
    } else if (accent_color_id.value() == cros_tokens::kCrosSysError) {
      warning_level =
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
    }
  }

  bool pinned = notification.rich_notification_data().pinned;

  if (pinned) {
    switch (warning_level) {
      case message_center::SystemNotificationWarningLevel::NORMAL:
        return CROS_SYSTEM_PRIORITY_PINNED;
      case message_center::SystemNotificationWarningLevel::WARNING:
        return CROS_SYSTEM_PRIORITY_WARNING_PINNED;
      case message_center::SystemNotificationWarningLevel::CRITICAL_WARNING:
        return CROS_SYSTEM_PRIORITY_CRITICAL_WARNING_PINNED;
    }
  } else {
    switch (warning_level) {
      case message_center::SystemNotificationWarningLevel::NORMAL:
        return CROS_SYSTEM_PRIORITY;
      case message_center::SystemNotificationWarningLevel::WARNING:
        return CROS_SYSTEM_PRIORITY_WARNING;
      case message_center::SystemNotificationWarningLevel::CRITICAL_WARNING:
        return CROS_SYSTEM_PRIORITY_CRITICAL_WARNING;
    }
  }
}

NotificationTypeDetailed GetNotificationTypeForCros(
    const message_center::Notification& notification) {
  int priority = notification.rich_notification_data().priority;
  if (priority == message_center::SYSTEM_PRIORITY)
    return GetNotificationTypeForCrosSystemPriority(notification);

  bool pinned = notification.rich_notification_data().pinned;

  if (pinned) {
    switch (priority) {
      case -2:
        return CROS_PRIORITY_MINUS_TWO_PINNED;
      case -1:
        return CROS_PRIORITY_MINUS_ONE_PINNED;
      case 0:
        return CROS_PRIORITY_ZERO_PINNED;
      case 1:
        return CROS_PRIORITY_ONE_PINNED;
      case 2:
        return CROS_PRIORITY_TWO_PINNED;
      default:
        NOTREACHED();
    }
  } else {
    switch (priority) {
      case -2:
        return CROS_PRIORITY_MINUS_TWO;
      case -1:
        return CROS_PRIORITY_MINUS_ONE;
      case 0:
        return CROS_PRIORITY_ZERO;
      case 1:
        return CROS_PRIORITY_ONE;
      case 2:
        return CROS_PRIORITY_TWO;
      default:
        NOTREACHED();
    }
  }
}

NotificationTypeDetailed GetNotificationTypeForWeb(
    const message_center::Notification& notification) {
  bool require_interaction =
      notification.rich_notification_data().never_timeout;
  return require_interaction ? WEB_REQUIRE_INTERACTION : WEB;
}

NotificationTypeDetailed GetNotificationTypeForPhoneHub(
    const message_center::Notification& notification) {
  int priority = notification.rich_notification_data().priority;
  switch (priority) {
    case -2:
      return PHONEHUB_PRIORITY_MINUS_TWO;
    case -1:
      return PHONEHUB_PRIORITY_MINUS_ONE;
    case 0:
      return PHONEHUB_PRIORITY_ZERO;
    case 1:
      return PHONEHUB_PRIORITY_ONE;
    case 2:
      return PHONEHUB_PRIORITY_TWO;
    default:
      NOTREACHED();
  }
}

NotificationTypeDetailed GetNotificationType(
    const message_center::Notification& notification) {
  message_center::NotifierType notifier_type = notification.notifier_id().type;
  switch (notifier_type) {
    case message_center::NotifierType::APPLICATION:
      return GetNotificationTypeForChromeApp(notification);
    case message_center::NotifierType::ARC_APPLICATION:
      return GetNotificationTypeForArc(notification);
    case message_center::NotifierType::WEB_PAGE:
      return GetNotificationTypeForWeb(notification);
    case message_center::NotifierType::SYSTEM_COMPONENT:
      return GetNotificationTypeForCros(notification);
    case message_center::NotifierType::PHONE_HUB:
      return GetNotificationTypeForPhoneHub(notification);
    case message_center::NotifierType::CROSTINI_APPLICATION:
      return CROSTINI;
    default:
      return OTHER;
  }
}

std::optional<NotificationViewType> GetNotificationViewType(
    message_center::Notification* notification) {
  std::optional<NotificationViewType> type;

  // Ignore ARC notification since its view is rendered through Android, and
  // its notification metadata for image and buttons is empty. Also ignore group
  // parent notification since it only serves as a placeholder.
  if (!notification ||
      notification->notifier_id().type ==
          message_center::NotifierType::ARC_APPLICATION ||
      notification->group_parent())
    return type;

  bool has_inline_reply = false;
  for (auto button_info : notification->buttons()) {
    if (button_info.placeholder) {
      has_inline_reply = true;
      break;
    }
  }

  bool has_buttons = !notification->buttons().empty();
  bool has_image = !notification->image().IsEmpty();
  bool is_grouped_child = notification->group_child();

  if (has_inline_reply) {
    if (has_image) {
      type = is_grouped_child
                 ? NotificationViewType::GROUPED_HAS_IMAGE_AND_INLINE_REPLY
                 : NotificationViewType::HAS_IMAGE_AND_INLINE_REPLY;
    } else {
      type = is_grouped_child ? NotificationViewType::GROUPED_HAS_INLINE_REPLY
                              : NotificationViewType::HAS_INLINE_REPLY;
    }
    return type;
  }

  if (has_buttons) {
    if (has_image) {
      type = is_grouped_child
                 ? NotificationViewType::GROUPED_HAS_IMAGE_AND_ACTION
                 : NotificationViewType::HAS_IMAGE_AND_ACTION;
    } else {
      type = is_grouped_child ? NotificationViewType::GROUPED_HAS_ACTION
                              : NotificationViewType::HAS_ACTION;
    }
    return type;
  }

  if (has_image) {
    return is_grouped_child ? NotificationViewType::GROUPED_HAS_IMAGE
                            : NotificationViewType::HAS_IMAGE;
  } else {
    return is_grouped_child ? NotificationViewType::GROUPED_SIMPLE
                            : NotificationViewType::SIMPLE;
  }
}

std::optional<NotificationTypeDetailed> GetNotificationType(
    const std::string& notification_id) {
  std::optional<NotificationTypeDetailed> type;
  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);
  if (!notification)
    return type;
  type = GetNotificationType(*notification);
  return type;
}

void LogHover(const std::string& notification_id, bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    base::UmaHistogramEnumeration("Notifications.Cros.Actions.Popup.Hover",
                                  type.value());
  } else {
    base::UmaHistogramEnumeration("Notifications.Cros.Actions.Tray.Hover",
                                  type.value());
  }
}

void LogClickedBody(const std::string& notification_id, bool is_popup) {
  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);
  if (!notification)
    return;
  const auto type = GetNotificationType(*notification);
  const auto catalog_name = notification->notifier_id().catalog_name;

  if (is_popup) {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.Popup.ClickedBody", type);
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.Popup.ClickedBody.GroupedByCatalog",
        catalog_name);
  } else {
    base::UmaHistogramEnumeration("Notifications.Cros.Actions.Tray.ClickedBody",
                                  type);
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.Tray.ClickedBody.GroupedByCatalog",
        catalog_name);
  }

  // If notification's delegate is null, that means the notification is not
  // clickable and the user just did a "bad click", which is a click that did
  // not do anything.
  if (notification->delegate()) {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.ClickedBody.GoodClick", type);
  } else {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.ClickedBody.BadClick", type);
  }
}

void LogClickedActionButton(const std::string& notification_id,
                            bool is_popup,
                            int button_index) {
  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);
  if (!notification) {
    return;
  }
  const auto type = GetNotificationType(*notification);
  if (is_popup) {
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Cros.Actions.Popup.ClickedActionButton", type);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Cros.Actions.Tray.ClickedActionButton", type);
  }

  if (!ValidCatalogName(notification->notifier_id())) {
    return;
  }

  const std::string histogram_base =
      GetNotifierFrameworkNotificationHistogramBase(notification->pinned());
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.ClickedActionButton.%u", histogram_base.c_str(),
                         button_index + 1),
      notification->notifier_id().catalog_name);
}

void LogInlineReplySent(const std::string& notification_id, bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.Popup.InlineReplySent", type.value());
  } else {
    base::UmaHistogramEnumeration(
        "Notifications.Cros.Actions.Tray.InlineReplySent", type.value());
  }
}

void LogPopupExpiredToTray(const std::string& notification_id) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("Notifications.Cros.Actions.Popup.ExpireToTray",
                            type.value());
}

void LogClosedByUser(const std::string& notification_id,
                     bool is_swipe,
                     bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    if (is_swipe) {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Popup.ClosedByUser.Swipe", type.value());
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Popup.ClosedByUser.Click", type.value());
    }
  } else {
    if (is_swipe) {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.ClosedByUser.Swipe", type.value());
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.ClosedByUser.Click", type.value());
    }
  }
}

void LogSettingsShown(const std::string& notification_id,
                      bool is_slide_controls,
                      bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    DCHECK(!is_slide_controls);
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Cros.Actions.Popup.SettingsShown.HoverControls",
        type.value());
  } else {
    if (is_slide_controls) {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.SettingsShown.SlideControls",
          type.value());
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.SettingsShown.HoverControls",
          type.value());
    }
  }
}

void LogSnoozed(const std::string& notification_id,
                bool is_slide_controls,
                bool is_popup) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  if (is_popup) {
    DCHECK(!is_slide_controls);
    UMA_HISTOGRAM_ENUMERATION(
        "Notifications.Cros.Actions.Popup.Snoozed.HoverControls", type.value());
  } else {
    if (is_slide_controls) {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.Snoozed.SlideControls",
          type.value());
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Cros.Actions.Tray.Snoozed.HoverControls",
          type.value());
    }
  }
}

void LogPopupShown(const std::string& notification_id) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("Notifications.Cros.Actions.Popup.Shown",
                            type.value());

  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  if (!notification)
    return;

  if (!ValidCatalogName(notification->notifier_id()))
    return;

  const std::string histogram_base =
      GetNotifierFrameworkNotificationHistogramBase(notification->pinned());

  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.Popup.ShownCount", histogram_base.c_str()),
      notification->notifier_id().catalog_name);
}

void LogPopupClosed(message_center::MessagePopupView* popup) {
  const message_center::NotifierId notifier_id =
      popup->message_view()->notifier_id();

  if (!ValidCatalogName(notifier_id))
    return;

  const std::string histogram_base =
      GetNotifierFrameworkNotificationHistogramBase(
          popup->message_view()->pinned());
  const base::TimeDelta user_journey_time =
      base::Time::Now() - popup->message_view()->timestamp();
  const std::string time_range =
      GetNotifierFrameworkPopupDismissedTimeRange(user_journey_time);

  base::UmaHistogramMediumTimes(
      base::StringPrintf("%s.Popup.UserJourneyTime", histogram_base.c_str()),
      user_journey_time);

  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.Popup.Dismissed.%s", histogram_base.c_str(),
                         time_range.c_str()),
      notifier_id.catalog_name);
}

void LogClosedByClearAll(const std::string& notification_id) {
  auto type = GetNotificationType(notification_id);
  if (!type.has_value())
    return;

  UMA_HISTOGRAM_ENUMERATION("Notifications.Cros.Actions.Tray.ClosedByClearAll",
                            type.value());
}

void LogNotificationAdded(const std::string& notification_id) {
  LogSystemNotificationAdded(notification_id);

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);
  if (!notification)
    return;

  base::UmaHistogramEnumeration("Notifications.Cros.Actions.NotificationAdded",
                                GetNotificationType(*notification));
  base::UmaHistogramEnumeration(
      "Notifications.Cros.Actions.NotificationAdded.GroupedByCatalog",
      notification->notifier_id().catalog_name);

  auto notification_view_type = GetNotificationViewType(notification);
  if (!notification_view_type)
    return;

  base::UmaHistogramEnumeration("Ash.NotificationView.NotificationAdded.Type",
                                notification_view_type.value());
}

void LogSystemNotificationAdded(const std::string& notification_id) {
  auto* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  if (!notification)
    return;

  if (!ValidCatalogName(notification->notifier_id()))
    return;

  const std::string histogram_base =
      GetNotifierFrameworkNotificationHistogramBase(notification->pinned());

  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.Added", histogram_base.c_str()),
      notification->notifier_id().catalog_name);
}

void LogNotificationsShownInFirstMinute(int count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "Notifications.Cros.Actions.CountOfNotificationShownInFirstMinutePerUser",
      count);
}

void LogCountOfNotificationsInOneGroup(int notification_count) {
  // `notification_count` might be zero if the parent doesn't have any child
  // notification left after remove.
  if (notification_count <= 0)
    return;
  base::UmaHistogramCounts100("Ash.Notification.CountOfNotificationsInOneGroup",
                              notification_count);
}

void LogExpandButtonClickAction(ExpandButtonClickAction action) {
  base::UmaHistogramEnumeration("Ash.NotificationView.ExpandButton.ClickAction",
                                action);
}

void LogGroupNotificationAddedType(GroupNotificationType type) {
  base::UmaHistogramEnumeration("Ash.Notification.GroupNotificationAdded",
                                type);
}

void LogOngoingProcessShownWithoutIcon(NotificationCatalogName catalog_name) {
  base::UmaHistogramEnumeration(
      "Ash.NotifierFramework.PinnedSystemNotification.ShownWithoutIcon",
      catalog_name);
}

void LogOngoingProcessShownWithoutTitle(NotificationCatalogName catalog_name) {
  base::UmaHistogramEnumeration(
      "Ash.NotifierFramework.PinnedSystemNotification.ShownWithoutTitle",
      catalog_name);
}

}  // namespace metrics_utils
}  // namespace ash
