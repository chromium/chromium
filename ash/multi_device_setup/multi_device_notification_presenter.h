// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MULTI_DEVICE_SETUP_MULTI_DEVICE_NOTIFICATION_PRESENTER_H_
#define ASH_MULTI_DEVICE_SETUP_MULTI_DEVICE_NOTIFICATION_PRESENTER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {
class MessageCenter;
class Notification;
class RichNotificationData;
}  // namespace message_center

namespace ash {

// Presents notifications necessary for MultiDevice setup flow. It observes the
// MultiDeviceSetup mojo service to show a notification when
// (1) a potential host is found for someone who has not gone through the setup
//     flow before,
// (2) the host has switched for someone who has, or
// (3) a new Chromebook has been added to an account for someone who has.
//
// The behavior caused by clicking a notification depends its content as
// described above:
// (1) triggers the setup UI to appear to prompt setup flow and
// (2) & (3) open the Connected Devices subpage in Settings.
//
// Note that if one notification is showing and another one is triggered, the
// old text is replaced (if it's different) and the notification pops up again.
class ASH_EXPORT MultiDeviceNotificationPresenter
    : public multidevice_setup::mojom::AccountStatusChangeDelegate,
      public SessionObserver,
      public message_center::MessageCenterObserver {
 public:
  explicit MultiDeviceNotificationPresenter(
      message_center::MessageCenter* message_center);

  MultiDeviceNotificationPresenter(const MultiDeviceNotificationPresenter&) =
      delete;
  MultiDeviceNotificationPresenter& operator=(
      const MultiDeviceNotificationPresenter&) = delete;

  ~MultiDeviceNotificationPresenter() override;

  // Disables notifications for tests.
  static std::unique_ptr<base::AutoReset<bool>>
  DisableNotificationsForTesting();

  // Removes the notification created by NotifyPotentialHostExists() or does
  // nothing if that notification is not currently displayed.
  void RemoveMultiDeviceSetupNotification();

  void UpdateIsSetupNotificationInteracted(
      bool is_setup_notificaton_interacted);

  // MultiDevice setup notification ID. Public so it can be accessed from
  // phone_hub_tray.cc
  static const char kSetupNotificationId[];

 protected:
  // multidevice_setup::mojom::AccountStatusChangeDelegate:
  void OnPotentialHostExistsForNewUser() override;
  void OnNoLongerNewUser() override;
  void OnConnectedHostSwitchedForExistingUser(
      const std::string& new_host_device_name) override;
  void OnNewChromebookAddedForExistingUser(
      const std::string& new_host_device_name) override;
  void OnBecameEligibleForWifiSync() override;

  // SessionObserver:
  void OnUserSessionAdded(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // message_center::MessageCenterObserver
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

  void OnNotificationClicked(
      const std::string& notification_id,
      const std::optional<int>& button_index,
      const std::optional<std::u16string>& reply) override;

 private:
  friend class MultiDeviceNotificationPresenterTest;

  // MultiDevice setup notification ID.
  static const char kWifiSyncNotificationId[];

  // Represents each possible MultiDevice setup notification that the setup flow
  // can show with a "none" option for the general state with no notification
  // present.
  enum class Status {
    kNoNotificationVisible,
    kNewUserNotificationVisible,
    kExistingUserHostSwitchedNotificationVisible,
    kExistingUserNewChromebookNotificationVisible
  };

  // Reflects MultiDeviceSetupNotification enum in enums.xml. Do not
  // rearrange.
  enum class NotificationType {
    kNewUserPotentialHostExists = 0,
    kExistingUserHostSwitched = 1,
    kExistingUserNewChromebookAdded = 2,
    // This is a legacy error case that is not expected to occur.
    kErrorUnknown = 3,
    kWifiSyncAnnouncement = 4,
    kMaxValue = kWifiSyncAnnouncement
  };

  static NotificationType GetMetricValueForNotification(
      Status notification_status);

  static std::string GetNotificationDescriptionForLogging(
      Status notification_status);

  void ObserveMultiDeviceSetupIfPossible();
  void ShowSetupNotification(const Status notification_status,
                             const std::u16string& title,
                             const std::u16string& message);
  void ShowNotification(const std::string& id,
                        const std::u16string& title,
                        const std::u16string& message,
                        message_center::RichNotificationData optional_fields);

  void FlushForTesting();

  // Indicates if Phone Hub icon is clicked when the setup notification is
  // visible. If the value is true, we do not log event to
  // MultiDevice.Setup.NotificationInteracted histogram.
  bool is_setup_notification_interacted_ = false;

  raw_ptr<message_center::MessageCenter> message_center_;

  // Notification currently showing or
  // Status::kNoNotificationVisible if there isn't one.
  Status notification_status_ = Status::kNoNotificationVisible;

  mojo::Remote<multidevice_setup::mojom::MultiDeviceSetup>
      multidevice_setup_remote_;
  mojo::Receiver<multidevice_setup::mojom::AccountStatusChangeDelegate>
      receiver_{this};

  base::WeakPtrFactory<MultiDeviceNotificationPresenter> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_MULTI_DEVICE_SETUP_MULTI_DEVICE_NOTIFICATION_PRESENTER_H_
