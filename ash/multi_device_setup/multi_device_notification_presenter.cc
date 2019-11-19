// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_device_setup/multi_device_notification_presenter.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

const char kNotifierMultiDevice[] = "ash.multi_device_setup";

}  // namespace

// static
const char MultiDeviceNotificationPresenter::kNotificationId[] =
    "cros_multi_device_setup_notification_id";

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
      return kNotificationTypeNewUserPotentialHostExists;
    case Status::kExistingUserHostSwitchedNotificationVisible:
      return kNotificationTypeExistingUserHostSwitched;
    case Status::kExistingUserNewChromebookNotificationVisible:
      return kNotificationTypeExistingUserNewChromebookAdded;
    case Status::kNoNotificationVisible:
      NOTREACHED();
      return kNotificationTypeMax;
  }
}

MultiDeviceNotificationPresenter::MultiDeviceNotificationPresenter(
    message_center::MessageCenter* message_center,
    service_manager::Connector* connector)
    : message_center_(message_center), connector_(connector) {
  DCHECK(message_center_);
  DCHECK(connector_);

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
  base::string16 title = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_TITLE);
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_MESSAGE,
      ui::GetChromeOSDeviceName());
  ShowNotification(Status::kNewUserNotificationVisible, title, message);
}

void MultiDeviceNotificationPresenter::OnNoLongerNewUser() {
  if (notification_status_ != Status::kNewUserNotificationVisible)
    return;
  RemoveMultiDeviceSetupNotification();
}

void MultiDeviceNotificationPresenter::OnConnectedHostSwitchedForExistingUser(
    const std::string& new_host_device_name) {
  base::string16 title = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_TITLE,
      base::ASCIIToUTF16(new_host_device_name));
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_MESSAGE,
      ui::GetChromeOSDeviceName());
  ShowNotification(Status::kExistingUserHostSwitchedNotificationVisible, title,
                   message);
}

void MultiDeviceNotificationPresenter::OnNewChromebookAddedForExistingUser(
    const std::string& new_host_device_name) {
  base::string16 title = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROME_DEVICE_ADDED_TITLE,
      base::ASCIIToUTF16(new_host_device_name));
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROME_DEVICE_ADDED_MESSAGE,
      ui::GetChromeOSDeviceName());
  ShowNotification(Status::kExistingUserNewChromebookNotificationVisible, title,
                   message);
}

void MultiDeviceNotificationPresenter::RemoveMultiDeviceSetupNotification() {
  notification_status_ = Status::kNoNotificationVisible;
  message_center_->RemoveNotification(kNotificationId,
                                      /* by_user */ false);
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
  if (by_user && notification_id == kNotificationId) {
    UMA_HISTOGRAM_ENUMERATION(
        "MultiDeviceSetup_NotificationDismissed",
        GetMetricValueForNotification(notification_status_),
        kNotificationTypeMax);
  }
}

void MultiDeviceNotificationPresenter::OnNotificationClicked(
    const std::string& notification_id,
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (notification_id != kNotificationId)
    return;

  DCHECK(notification_status_ != Status::kNoNotificationVisible);
  PA_LOG(VERBOSE) << "User clicked "
                  << GetNotificationDescriptionForLogging(notification_status_)
                  << ".";
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup_NotificationClicked",
                            GetMetricValueForNotification(notification_status_),
                            kNotificationTypeMax);
  switch (notification_status_) {
    case Status::kNewUserNotificationVisible:
      Shell::Get()->system_tray_model()->client()->ShowMultiDeviceSetup();
      break;
    case Status::kExistingUserHostSwitchedNotificationVisible:
      // Clicks on the 'host switched' and 'Chromebook added' notifications have
      // the same effect, i.e. opening the Settings subpage.
      FALLTHROUGH;
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

  // If no active user is logged in, there is nothing to do.
  if (session_controller->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  const UserSession* user_session = session_controller->GetPrimaryUserSession();

  // The primary user session may be unavailable (e.g., for test/guest users).
  if (!user_session)
    return;

  base::Optional<base::Token> service_instance_group =
      user_session->user_info.service_instance_group;

  // Cannot proceed if there is no known service instance group.
  if (!service_instance_group)
    return;

  connector_->Connect(service_manager::ServiceFilter::ByNameInGroup(
                          chromeos::multidevice_setup::mojom::kServiceName,
                          *service_instance_group),
                      multidevice_setup_remote_.BindNewPipeAndPassReceiver());

  // Add this object as the delegate of the MultiDeviceSetup Service.
  multidevice_setup_remote_->SetAccountStatusChangeDelegate(
      receiver_.BindNewPipeAndPassRemote());

  message_center_->AddObserver(this);
}

void MultiDeviceNotificationPresenter::ShowNotification(
    const Status notification_status,
    const base::string16& title,
    const base::string16& message) {
  PA_LOG(VERBOSE) << "Showing "
                  << GetNotificationDescriptionForLogging(notification_status)
                  << ".";
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup_NotificationShown",
                            GetMetricValueForNotification(notification_status),
                            kNotificationTypeMax);
  if (message_center_->FindVisibleNotificationById(kNotificationId)) {
    message_center_->UpdateNotification(kNotificationId,
                                        CreateNotification(title, message));
  } else {
    message_center_->AddNotification(CreateNotification(title, message));
  }
  notification_status_ = notification_status;
}

std::unique_ptr<message_center::Notification>
MultiDeviceNotificationPresenter::CreateNotification(
    const base::string16& title,
    const base::string16& message) {
  return CreateSystemNotification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId, title, message, base::string16() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierMultiDevice),
      message_center::RichNotificationData(), nullptr /* delegate */,
      kNotificationMultiDeviceSetupIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

void MultiDeviceNotificationPresenter::FlushForTesting() {
  if (multidevice_setup_remote_)
    multidevice_setup_remote_.FlushForTesting();
}

}  // namespace ash
