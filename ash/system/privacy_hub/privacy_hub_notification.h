// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

// A custom delegate that ensures consistent handling of notification
// interactions across all Privacy Hub notifications.
class ASH_EXPORT PrivacyHubNotificationClickDelegate
    : public message_center::NotificationDelegate {
 public:
  // The `button_callback` will be executed when the only button of the
  // notification is clicked.
  explicit PrivacyHubNotificationClickDelegate(
      base::RepeatingClosure button_click);

  // message_center::NotificationDelegate:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

  // When clicking on the notification message execute this `callback`.
  void SetMessageClickCallback(base::RepeatingClosure callback);

 private:
  ~PrivacyHubNotificationClickDelegate() override;

  base::RepeatingClosure button_callback_;
  base::RepeatingClosure message_callback_;
};

// This class wraps `SystemNotificationBuilder` and adds additional constraints
// and shared behavior that applies to all Privacy Hub notifications.
class ASH_EXPORT PrivacyHubNotification {
 public:
  using MessageIds = std::vector<int>;
  using SensorSet =
      base::EnumSet<SensorDisabledNotificationDelegate::Sensor,
                    SensorDisabledNotificationDelegate::Sensor::kMinValue,
                    SensorDisabledNotificationDelegate::Sensor::kMaxValue>;

  constexpr static base::TimeDelta kMinShowTime =
      base::Seconds(message_center::kAutocloseDefaultDelaySeconds);

  // Create a new notification. When calling `Show()` and `sensors_for_apps`
  // contains at least one sensor it will try to replace currently used apps
  // by the sensor(s) in the message. This is only possible if there are less
  // than `message_ids.size()` apps active for the sensor(s) otherwise the
  // generic message at index 0 will be used again. `message_ids` must not be
  // empty and `delegate` must not be null.
  PrivacyHubNotification(
      const std::string& id,
      int title_id,
      const MessageIds& message_ids,
      SensorSet sensors_for_apps,
      scoped_refptr<PrivacyHubNotificationClickDelegate> delegate,
      ash::NotificationCatalogName catalog_name,
      int action_button_id);
  PrivacyHubNotification(PrivacyHubNotification&&) = delete;
  PrivacyHubNotification& operator=(PrivacyHubNotification&&) = delete;
  ~PrivacyHubNotification();

  // Show the notification to the user for at least `kMinShowTime`. Calls to
  // `Hide()` are delayed until this time has passed and the notification is
  // hidden then. If more than one `message_ids_` exists will attempt to use
  // the correct one for the number of apps accessing the `sensors_for_apps_`.
  void Show();

  // Hide the notification from the user if it has already been shown for at
  // least `kMinShowTime`. If not the notification will be shown for the
  // remaining time and then hidden.
  void Hide();

  // Silently updates the notification when needed, for example, when an
  // application stops accessing a sensor and the name of that application needs
  // to be removed from the notification without letting the notification pop up
  // again.
  void Update();

  // Get the underlying `SystemNotificationBuilder` to do modifications beyond
  // what this wrapper allows you to do. If you change the ID of the message
  // `Show()` and `Hide()` are not going to work reliably.
  SystemNotificationBuilder& builder() { return builder_; }

 private:
  // Get names of apps accessing the `sensors_for_apps_`. At most
  // `message_ids_.size()` elements will be returned.
  std::vector<std::u16string> GetAppsAccessingSensors() const;

  // Sets the notification message depending on the list of apps accessing the
  // `sensors_for_apps_`.
  void SetNotificationMessage();

  std::string id_;
  SystemNotificationBuilder builder_;
  MessageIds message_ids_;
  SensorSet sensors_for_apps_;
  absl::optional<base::Time> last_time_shown_;
  base::OneShotTimer remove_timer_;
};

}  // namespace ash

#endif
