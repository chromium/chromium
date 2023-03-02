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

  // Set the `callback` for an additional button.
  void SetSecondButtonCallback(base::RepeatingClosure callback);

 private:
  ~PrivacyHubNotificationClickDelegate() override;

  // Run `callback` if it's not null. Do nothing otherwise.
  void RunCallbackIfNotNull(const base::RepeatingClosure& callback);

  std::array<base::RepeatingClosure, 2> button_callbacks_;
  base::RepeatingClosure message_callback_;
};

// Represents the information displayed in a `PrivacyHubNotification`.
class ASH_EXPORT PrivacyHubNotificationDescriptor {
  // `message_ids` must not be empty` and `delegate` must not be null.
  // `message_ids.at(0)` should be the generic notification message with no
  // application names.
  // `message_ids.at(n)` should be the notification message with `n` application
  // names.
 public:
  PrivacyHubNotificationDescriptor(
      const SensorDisabledNotificationDelegate::SensorSet& sensors,
      int title_id,
      int button_id,
      const std::vector<int>& message_ids,
      scoped_refptr<PrivacyHubNotificationClickDelegate> delegate);
  PrivacyHubNotificationDescriptor(
      const PrivacyHubNotificationDescriptor& other);
  PrivacyHubNotificationDescriptor& operator=(
      const PrivacyHubNotificationDescriptor& other);
  ~PrivacyHubNotificationDescriptor();

  const SensorDisabledNotificationDelegate::SensorSet& sensors() const {
    return sensors_;
  }

  const std::vector<int>& message_ids() const { return message_ids_; }

  scoped_refptr<PrivacyHubNotificationClickDelegate> delegate() const {
    return delegate_;
  }

  int title_id_;
  int button_id_;

 private:
  SensorDisabledNotificationDelegate::SensorSet sensors_;
  std::vector<int> message_ids_;
  scoped_refptr<PrivacyHubNotificationClickDelegate> delegate_;
};

// This class wraps `SystemNotificationBuilder` and adds additional constraints
// and shared behavior that applies to all Privacy Hub notifications.
class ASH_EXPORT PrivacyHubNotification {
 public:
  constexpr static base::TimeDelta kMinShowTime =
      base::Seconds(message_center::kAutocloseDefaultDelaySeconds);

  // Create a new notification.
  // When calling `Show() or `Update()`:
  // If `sensors_` is empty, the generic notification message will be displayed.
  // If `sensors_` is non-empty and `n` applications are using the sensors in
  // `sensors_`, the displayed notification message will contain `n` application
  // names. If a notification message with `n` application names is not
  // provided, the generic notification message will be displayed.
  PrivacyHubNotification(const std::string& id,
                         NotificationCatalogName catalog_name,
                         const PrivacyHubNotificationDescriptor& descriptor);
  PrivacyHubNotification(PrivacyHubNotification&&) = delete;
  PrivacyHubNotification& operator=(PrivacyHubNotification&&) = delete;
  ~PrivacyHubNotification();

  // Show the notification to the user for at least `kMinShowTime`. Every time
  // `Show()` is called, the notification will pop up. For silent updates, use
  // the `Update()` function. Calls to `Hide()` are delayed until `kMinShowTime`
  // time has passed and the notification is hidden then.
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

  // Add an additional button to the notification. The button title will be
  // generated from the `title_id`. Clicking the button will invoke the
  // `callback`. Only one additional button can be active at the same time.
  void SetSecondButton(base::RepeatingClosure callback, int title_id);

  // Get the underlying `SystemNotificationBuilder` to do modifications beyond
  // what this wrapper allows you to do. If you change the ID of the message
  // `Show()` and `Hide()` are not going to work reliably.
  SystemNotificationBuilder& builder() { return builder_; }

 private:
  // Get names of apps accessing sensors in `sensors_`. At most
  // `message_ids_.size()` elements will be returned.
  std::vector<std::u16string> GetAppsAccessingSensors() const;

  // Sets the content(message, title, buttons etc.) of the notification
  // depending on the values of `sensors_` and `message_ids_`.
  void SetNotificationContent();

  // Create an object of optional data fields with the defaults applying to
  // every Privacy Hub notification.
  message_center::RichNotificationData MakeOptionalFields() const;

  std::string id_;
  SystemNotificationBuilder builder_;
  std::vector<int> message_ids_;
  SensorDisabledNotificationDelegate::SensorSet sensors_;
  absl::optional<base::Time> last_time_shown_;
  base::OneShotTimer remove_timer_;
  scoped_refptr<PrivacyHubNotificationClickDelegate> delegate_;
  std::u16string button_text_;
};

}  // namespace ash

#endif
