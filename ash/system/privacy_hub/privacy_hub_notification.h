// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ui/message_center/message_center_observer.h"
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
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

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
  // `message_ids` must not be empty`.
  // `message_ids.at(0)` should be the generic notification message with no
  // application names.
  // `message_ids.at(n)` should be the notification message with `n` application
  // names.
  // `delegate` specifies the callback to be used when the button is clicked.
  // if it is null (default), an action that sets all privacy toggles
  // corresponding `sensors` to true.
 public:
  PrivacyHubNotificationDescriptor(
      const SensorDisabledNotificationDelegate::SensorSet& sensors,
      int title_id,
      const std::vector<int>& button_ids,
      const std::vector<int>& message_ids,
      scoped_refptr<PrivacyHubNotificationClickDelegate> delegate = nullptr);
  PrivacyHubNotificationDescriptor(
      const PrivacyHubNotificationDescriptor& other);
  PrivacyHubNotificationDescriptor& operator=(
      const PrivacyHubNotificationDescriptor& other);
  ~PrivacyHubNotificationDescriptor();

  const std::vector<int>& button_ids() const { return button_ids_; }

  const SensorDisabledNotificationDelegate::SensorSet& sensors() const {
    return sensors_;
  }

  const std::vector<int>& message_ids() const { return message_ids_; }

  scoped_refptr<PrivacyHubNotificationClickDelegate> delegate() const {
    return delegate_;
  }

  int title_id_;

 private:
  std::vector<int> button_ids_;
  SensorDisabledNotificationDelegate::SensorSet sensors_;
  std::vector<int> message_ids_;
  scoped_refptr<PrivacyHubNotificationClickDelegate> delegate_;
};

// This class wraps `SystemNotificationBuilder` and adds additional constraints
// and shared behavior that applies to all Privacy Hub notifications.
class ASH_EXPORT PrivacyHubNotification
    : public message_center::MessageCenterObserver {
 public:
  // Class used to encapsulate the logic whether a notification should be
  // throttled (suppressed because it has been manually dismissed recently);
  class Throttler {
   public:
    Throttler() = default;
    Throttler(const Throttler&) = delete;
    Throttler& operator=(const Throttler&) = delete;
    virtual ~Throttler() = default;

    // Returns `true` if the notification should be suppressed.
    virtual bool ShouldThrottle() = 0;
    // To be called when a notification is dismissed by the user.
    virtual void RecordDismissalByUser() = 0;
  };

  // Create a new notification.
  // When calling `Show() or `Update()`:
  // If `sensors_` is empty, the generic notification message from `descriptor`
  // will be displayed.
  // If `sensors_` is non-empty and `n` applications are using the sensors in
  // `sensors_`, the displayed notification message will contain `n` application
  // names. If `descriptor` does not contain a notification message with `n`
  // application names, the generic notification message from `descriptor` will
  // be displayed.
  PrivacyHubNotification(const std::string& id,
                         NotificationCatalogName catalog_name,
                         const PrivacyHubNotificationDescriptor& descriptor);

  // When PrivacyHubNotification is constructed with multiple
  // `PrivacyHubNotificationDescriptor`s, which descriptor to use will be
  // decided depending on the value of `sensors_`. When `sensors_` changes, the
  // descriptor to use will also change.
  //`descriptors` must have multiple `PrivacyHubNotificationDescriptor` objects,
  // use the previous constructor otherwise please.
  PrivacyHubNotification(
      const std::string& id,
      NotificationCatalogName catalog_name,
      const std::vector<PrivacyHubNotificationDescriptor>& descriptors);

  PrivacyHubNotification(PrivacyHubNotification&&) = delete;
  PrivacyHubNotification& operator=(PrivacyHubNotification&&) = delete;

  ~PrivacyHubNotification() override;

  // message_center::MessageCenterObserver:
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

  // Show the notification to the user for at least `kMinShowTime`. Every time
  // `Show()` is called, the notification will pop up. For silent updates, use
  // the `Update()` function.
  void Show();

  // Hide the notification from the message center.
  void Hide();

  // Returns true if this notificaiton is shown (present in the message center).
  bool IsShown();

  // Silently updates the notification when needed, for example, when an
  // application stops accessing a sensor and the name of that application needs
  // to be removed from the notification without letting the notification pop up
  // again.
  void Update();

  // Updates priority for notification that will be created via Show/Update.
  void SetPriority(message_center::NotificationPriority priority);

  // Updates the value of `sensors_`.
  void SetSensors(SensorDisabledNotificationDelegate::SensorSet sensors);

  // Get the underlying `SystemNotificationBuilder` to do modifications beyond
  // what this wrapper allows you to do. If you change the ID of the message
  // `Show()` and `Hide()` are not going to work reliably.
  SystemNotificationBuilder& builder() { return builder_; }

  // Sets a custom Throttler - the object that decides whether to suppress a
  // notification due too too many repetitions.
  void SetThrottler(std::unique_ptr<Throttler> throttler);

 private:
  // Starts observation of dismissed messages
  void StartDismissalObservation();
  // Stops observation of dismissed messages
  void StopDismissalObservation();

  // Get names of apps accessing sensors in `sensors_`. At most `number_of_apps`
  // elements will be returned.
  std::vector<std::u16string> GetAppsAccessingSensors(
      size_t number_of_apps) const;

  // Propagates information about the update in notification content (message,
  // title, buttons etc.) to the underlying `SystemNotificationBuilder`. This is
  // always done before showing or updating a notification.
  void SetNotificationContent();

  std::string id_;

  // A set of `PrivacyHubNotificationDescriptor`s. Appropriate
  // `PrivacyHubNotificationDescriptor` for a specific `SensorSet` can be found
  // using the standard `find` function. `sensors_.ToEnumBitmask()` can be used
  // as the key for the `find` function.
  std::set<PrivacyHubNotificationDescriptor, std::less<>>
      notification_descriptors_;

  SensorDisabledNotificationDelegate::SensorSet sensors_;

  // `notification_descriptors_` is a set of
  // `PrivacyHubNotificationDescriptor`s. The content in the descriptors are
  // used to update the underlying `SystemNotificationBuilder`. Before a call to
  // `Show()` or `Update()`, the underlying builder needs to be updated. Content
  // of which descriptor to use to update the builder depends on the value
  // current value of `sensors_` enumset.
  // `has_sensors_changed_` being true means that `sensors_` was updated but the
  // underlying builder was not updated after that.
  bool has_sensors_changed_ = true;

  SystemNotificationBuilder builder_;

  // TODO(b/271809217): Refactor camera HW switch notification implementation
  // Notification for the camera hardware switch is currently using only a
  // subset of `PrivacyHubNotification` properties. `catalog_name_` is stored to
  // determine if the notification is for the camera hardware switch to handle
  // it specially.
  NotificationCatalogName catalog_name_;

  // Encapsulates the throttling logic for this notification.
  std::unique_ptr<Throttler> throttler_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_H_
