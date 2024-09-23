// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_device_setup/multi_device_notification_presenter.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/util/histogram_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

bool g_disable_notifications_for_test_ = false;

const char kNotifierMultiDevice[] = "ash.multi_device_setup";

}  // namespace

// static
const char MultiDeviceNotificationPresenter::kSetupNotificationId[] =
    "cros_multi_device_setup_notification_id";

// static
const char MultiDeviceNotificationPresenter::kWifiSyncNotificationId[] =
    "cros_wifi_sync_announcement_notification_id";

// static
std::string
MultiDeviceNotificationPresenter::GetNotificationDescriptionForLogging(
    Status notification_status) {
  switch (notification_status) {
    case Status::kNewUserNotificationVisible:
      return "notification to prompt setup";
    case Status::kExistingUserHostSwitchedNotificationVisible:
      return "notification of switch to new host";
    case Status::kExistingUserNewChromebookNotificationVisible:
      return "notification of new Chromebook added";
    case Status::kNoNotificationVisible:
      return "no notification";
  }
  NOTREACHED();
}

// static
MultiDeviceNotificationPresenter::NotificationType
MultiDeviceNotificationPresenter::GetMetricValueForNotification(
    Status notification_status) {
  switch (notification_status) {
    case Status::kNewUserNotificationVisible:
      return NotificationType::kNewUserPotentialHostExists;
    case Status::kExistingUserHostSwitchedNotificationVisible:
      return NotificationType::kExistingUserHostSwitched;
    case Status::kExistingUserNewChromebookNotificationVisible:
      return NotificationType::kExistingUserNewChromebookAdded;
    case Status::kNoNotificationVisible:
      NOTREACHED();
  }
}

MultiDeviceNotificationPresenter::MultiDeviceNotificationPresenter(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  DCHECK(message_center_);

  Shell::Get()->session_controller()->AddObserver(this);

  // If the constructor is called after the session state has already been set
  // (e.g., if recovering from a crash), handle that now. If the user has not
  // yet logged in, this will be a no-op.
  ObserveMultiDeviceSetupIfPossible();
}

MultiDeviceNotificationPresenter::~MultiDeviceNotificationPresenter() {
  message_center_->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void MultiDeviceNotificationPresenter::OnPotentialHostExistsForNewUser() {
  int title_message_id =
      IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_TITLE;
  if (features::IsPhoneHubOnboardingNotifierRevampEnabled() &&
      !features::kPhoneHubOnboardingNotifierUseNudge.Get()) {
    title_message_id =
        features::kPhoneHubNotifierTextGroup.Get() ==
                features::PhoneHubNotifierTextGroup::kNotifierTextGroupA
            ? IDS_ASH_MULTI_DEVICE_SETUP_NOTIFIER_TEXT_WITH_PHONE_HUB
            : IDS_ASH_MULTI_DEVICE_SETUP_NOTIFIER_TEXT_WITHOUT_PHONE_HUB;
  }
  std::u16string title = l10n_util::GetStringUTF16(title_message_id);
  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_MESSAGE,
      ui::GetChromeOSDeviceName());
  ShowSetupNotification(Status::kNewUserNotificationVisible, title, message);
}

void MultiDeviceNotificationPresenter::OnNoLongerNewUser() {
  if (notification_status_ != Status::kNewUserNotificationVisible)
    return;
  RemoveMultiDeviceSetupNotification();
}

void MultiDeviceNotificationPresenter::OnConnectedHostSwitchedForExistingUser(
    const std::string& new_host_device_name) {
  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_TITLE,
      base::ASCIIToUTF16(new_host_device_name));
  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_MESSAGE,
      ui::GetChromeOSDeviceName());
  ShowSetupNotification(Status::kExistingUserHostSwitchedNotificationVisible,
                        title, message);
}

void MultiDeviceNotificationPresenter::OnNewChromebookAddedForExistingUser(
    const std::string& new_host_device_name) {
  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROME_DEVICE_ADDED_TITLE,
      base::ASCIIToUTF16(new_host_device_name));
  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROME_DEVICE_ADDED_MESSAGE,
      ui::GetChromeOSDeviceName());
  ShowSetupNotification(Status::kExistingUserNewChromebookNotificationVisible,
                        title, message);
}

void MultiDeviceNotificationPresenter::OnBecameEligibleForWifiSync() {
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_ASH_MULTI_DEVICE_WIFI_SYNC_AVAILABLE_TITLE);
  std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_WIFI_SYNC_AVAILABLE_MESSAGE,
      ui::GetChromeOSDeviceName());
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ASH_MULTI_DEVICE_WIFI_SYNC_AVAILABLE_TURN_ON_BUTTON)));
  optional_fields.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ASH_MULTI_DEVICE_WIFI_SYNC_AVAILABLE_CANCEL_BUTTON)));

  ShowNotification(kWifiSyncNotificationId, title, message, optional_fields);
  base::UmaHistogramEnumeration("MultiDeviceSetup_NotificationShown",
                                NotificationType::kWifiSyncAnnouncement);
}

// static
std::unique_ptr<base::AutoReset<bool>>
MultiDeviceNotificationPresenter::DisableNotificationsForTesting() {
  return std::make_unique<base::AutoReset<bool>>(
      &g_disable_notifications_for_test_, true);
}

void MultiDeviceNotificationPresenter::RemoveMultiDeviceSetupNotification() {
  notification_status_ = Status::kNoNotificationVisible;
  message_center_->RemoveNotification(kSetupNotificationId,
                                      /* by_user */ false);
}

void MultiDeviceNotificationPresenter::UpdateIsSetupNotificationInteracted(
    bool is_setup_notification_interacted) {
  is_setup_notification_interacted_ = is_setup_notification_interacted;
}

void MultiDeviceNotificationPresenter::OnUserSessionAdded(
    const AccountId& account_id) {
  ObserveMultiDeviceSetupIfPossible();
}

void MultiDeviceNotificationPresenter::OnSessionStateChanged(
    session_manager::SessionState state) {
  ObserveMultiDeviceSetupIfPossible();
}

void MultiDeviceNotificationPresenter::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (!by_user) {
    return;
  }
  if (notification_id == kSetupNotificationId) {
    base::UmaHistogramEnumeration(
        "MultiDeviceSetup_NotificationDismissed",
        GetMetricValueForNotification(notification_status_));
    return;
  }
  if (notification_id == kWifiSyncNotificationId) {
    base::UmaHistogramEnumeration("MultiDeviceSetup_NotificationDismissed",
                                  NotificationType::kWifiSyncAnnouncement);
    return;
  }
}

void MultiDeviceNotificationPresenter::OnNotificationClicked(
    const std::string& notification_id,
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (notification_id == kWifiSyncNotificationId) {
    message_center_->RemoveNotification(kWifiSyncNotificationId,
                                        /* by_user */ false);

    if (button_index) {
      switch (*button_index) {
        case 0:  // "Turn on" button
          PA_LOG(INFO) << "Enabling Wi-Fi Sync.";
          multidevice_setup_remote_->SetFeatureEnabledState(
              multidevice_setup::mojom::Feature::kWifiSync,
              /*enabled=*/true, /*auth_token=*/std::nullopt,
              /*callback=*/base::DoNothing());
          break;
        case 1:  // "Cancel" button
          base::UmaHistogramEnumeration(
              "MultiDeviceSetup_NotificationDismissed",
              NotificationType::kWifiSyncAnnouncement);
          return;
      }
    }

    Shell::Get()->system_tray_model()->client()->ShowWifiSyncSettings();
    base::UmaHistogramEnumeration("MultiDeviceSetup_NotificationClicked",
                                  NotificationType::kWifiSyncAnnouncement);
    return;
  }

  if (notification_id != kSetupNotificationId)
    return;

  DCHECK(notification_status_ != Status::kNoNotificationVisible);
  PA_LOG(VERBOSE) << "User clicked "
                  << GetNotificationDescriptionForLogging(notification_status_)
                  << ".";
  base::UmaHistogramEnumeration(
      "MultiDeviceSetup_NotificationClicked",
      GetMetricValueForNotification(notification_status_));
  switch (notification_status_) {
    case Status::kNewUserNotificationVisible:
      Shell::Get()->system_tray_model()->client()->ShowMultiDeviceSetup();
      phonehub::util::LogMultiDeviceSetupDialogEntryPoint(
          ash::phonehub::util::MultiDeviceSetupDialogEntrypoint::
              kSetupNotification);
      // If user has not interacted with Phone Hub icon when the notification is
      // visible, log MultiDeviceSetup.NotificationInteracted event when
      // notification is clicked.
      if (!is_setup_notification_interacted_) {
        base::UmaHistogramCounts100("MultiDeviceSetup.NotificationInteracted",
                                    1);
      } else {
        // Restore the value when the notification is clicked.
        UpdateIsSetupNotificationInteracted(false);
      }
      break;
    case Status::kExistingUserHostSwitchedNotificationVisible:
      // Clicks on the 'host switched' and 'Chromebook added' notifications have
      // the same effect, i.e. opening the Settings subpage.
      [[fallthrough]];
    case Status::kExistingUserNewChromebookNotificationVisible:
      Shell::Get()
          ->system_tray_model()
          ->client()
          ->ShowConnectedDevicesSettings();
      break;
    case Status::kNoNotificationVisible:
      NOTREACHED();
  }
  RemoveMultiDeviceSetupNotification();
}

void MultiDeviceNotificationPresenter::ObserveMultiDeviceSetupIfPossible() {
  // If already the delegate, there is nothing else to do.
  if (multidevice_setup_remote_)
    return;

  const SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();

  if (session_controller->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  const UserSession* user_session = session_controller->GetPrimaryUserSession();

  // The primary user session may be unavailable (e.g., for test/guest users).
  if (!user_session)
    return;

  Shell::Get()->shell_delegate()->BindMultiDeviceSetup(
      multidevice_setup_remote_.BindNewPipeAndPassReceiver());

  // Add this object as the delegate of the MultiDeviceSetup Service.
  multidevice_setup_remote_->SetAccountStatusChangeDelegate(
      receiver_.BindNewPipeAndPassRemote());

  message_center_->AddObserver(this);
}

void MultiDeviceNotificationPresenter::ShowSetupNotification(
    const Status notification_status,
    const std::u16string& title,
    const std::u16string& message) {
  PA_LOG(VERBOSE) << "Showing "
                  << GetNotificationDescriptionForLogging(notification_status)
                  << ".";
  base::UmaHistogramEnumeration(
      "MultiDeviceSetup_NotificationShown",
      GetMetricValueForNotification(notification_status));

  ShowNotification(kSetupNotificationId, title, message,
                   message_center::RichNotificationData());
  notification_status_ = notification_status;
}

void MultiDeviceNotificationPresenter::ShowNotification(
    const std::string& id,
    const std::u16string& title,
    const std::u16string& message,
    message_center::RichNotificationData optional_fields) {
  if (g_disable_notifications_for_test_)
    return;

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE, id, title,
          message, std::u16string() /* display_source */,
          GURL() /* origin_url */,
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierMultiDevice, NotificationCatalogName::kMultiDevice),
          optional_fields, nullptr /* delegate */,
          kNotificationMultiDeviceSetupIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  if (message_center_->FindVisibleNotificationById(kSetupNotificationId)) {
    message_center_->UpdateNotification(id, std::move(notification));
    return;
  }

  message_center_->AddNotification(std::move(notification));
}

void MultiDeviceNotificationPresenter::FlushForTesting() {
  if (multidevice_setup_remote_)
    multidevice_setup_remote_.FlushForTesting();
}

}  // namespace ash
