// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/phone_status_processor.h"

#include <algorithm>
#include <string>

#include "ash/components/phonehub/do_not_disturb_controller.h"
#include "ash/components/phonehub/find_my_device_controller.h"
#include "ash/components/phonehub/message_receiver.h"
#include "ash/components/phonehub/mutable_phone_model.h"
#include "ash/components/phonehub/notification_access_manager.h"
#include "ash/components/phonehub/notification_processor.h"
#include "ash/components/phonehub/screen_lock_manager_impl.h"
#include "ash/constants/ash_features.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {
namespace phonehub {

namespace {

using multidevice_setup::MultiDeviceSetupClient;

PhoneStatusModel::MobileStatus GetMobileStatusFromProto(
    proto::MobileConnectionState mobile_status) {
  switch (mobile_status) {
    case proto::MobileConnectionState::NO_SIM:
      return PhoneStatusModel::MobileStatus::kNoSim;
    case proto::MobileConnectionState::SIM_BUT_NO_RECEPTION:
      return PhoneStatusModel::MobileStatus::kSimButNoReception;
    case proto::MobileConnectionState::SIM_WITH_RECEPTION:
      return PhoneStatusModel::MobileStatus::kSimWithReception;
    default:
      return PhoneStatusModel::MobileStatus::kNoSim;
  }
}

PhoneStatusModel::SignalStrength GetSignalStrengthFromProto(
    proto::SignalStrength signal_strength) {
  switch (signal_strength) {
    case proto::SignalStrength::ZERO_BARS:
      return PhoneStatusModel::SignalStrength::kZeroBars;
    case proto::SignalStrength::ONE_BAR:
      return PhoneStatusModel::SignalStrength::kOneBar;
    case proto::SignalStrength::TWO_BARS:
      return PhoneStatusModel::SignalStrength::kTwoBars;
    case proto::SignalStrength::THREE_BARS:
      return PhoneStatusModel::SignalStrength::kThreeBars;
    case proto::SignalStrength::FOUR_BARS:
      return PhoneStatusModel::SignalStrength::kFourBars;
    default:
      return PhoneStatusModel::SignalStrength::kZeroBars;
  }
}

PhoneStatusModel::ChargingState GetChargingStateFromProto(
    proto::ChargingState charging_state) {
  switch (charging_state) {
    case proto::ChargingState::NOT_CHARGING:
      return PhoneStatusModel::ChargingState::kNotCharging;
    case proto::ChargingState::CHARGING_AC:
    case proto::ChargingState::CHARGING_WIRELESS:
      return PhoneStatusModel::ChargingState::kChargingAc;
    case proto::ChargingState::CHARGING_USB:
      return PhoneStatusModel::ChargingState::kChargingUsb;
    default:
      return PhoneStatusModel::ChargingState::kNotCharging;
  }
}

PhoneStatusModel::BatterySaverState GetBatterySaverStateFromProto(
    proto::BatteryMode battery_mode) {
  switch (battery_mode) {
    case proto::BatteryMode::BATTERY_SAVER_OFF:
      return PhoneStatusModel::BatterySaverState::kOff;
    case proto::BatteryMode::BATTERY_SAVER_ON:
      return PhoneStatusModel::BatterySaverState::kOn;
    default:
      return PhoneStatusModel::BatterySaverState::kOff;
  }
}

NotificationAccessManager::AccessStatus ComputeNotificationAccessState(
    const proto::PhoneProperties& phone_properties) {
  // If the user has a Work Profile active, notification access is not allowed
  // by Android. See https://crbug.com/1155151.
  if (phone_properties.profile_type() == proto::ProfileType::WORK_PROFILE)
    return NotificationAccessManager::AccessStatus::kProhibited;

  if (phone_properties.notification_access_state() ==
      proto::NotificationAccessState::ACCESS_GRANTED) {
    return NotificationAccessManager::AccessStatus::kAccessGranted;
  }

  return NotificationAccessManager::AccessStatus::kAvailableButNotGranted;
}

ScreenLockManager::LockStatus ComputeScreenLockState(
    const proto::PhoneProperties& phone_properties) {
  switch (phone_properties.screen_lock_state()) {
    case proto::ScreenLockState::SCREEN_LOCK_UNKNOWN:
      return ScreenLockManager::LockStatus::kUnknown;
    case proto::ScreenLockState::SCREEN_LOCK_OFF:
      return ScreenLockManager::LockStatus::kLockedOff;
    case proto::ScreenLockState::SCREEN_LOCK_ON:
      return ScreenLockManager::LockStatus::kLockedOn;
    default:
      return ScreenLockManager::LockStatus::kUnknown;
  }
}

FindMyDeviceController::Status ComputeFindMyDeviceStatus(
    const proto::PhoneProperties& phone_properties) {
  if (phone_properties.find_my_device_capability() ==
      proto::FindMyDeviceCapability::NOT_ALLOWED) {
    return FindMyDeviceController::Status::kRingingNotAvailable;
  }

  bool is_ringing =
      phone_properties.ring_status() == proto::FindMyDeviceRingStatus::RINGING;

  return is_ringing ? FindMyDeviceController::Status::kRingingOn
                    : FindMyDeviceController::Status::kRingingOff;
}

PhoneStatusModel CreatePhoneStatusModel(const proto::PhoneProperties& proto) {
  return PhoneStatusModel(
      GetMobileStatusFromProto(proto.connection_state()),
      PhoneStatusModel::MobileConnectionMetadata{
          GetSignalStrengthFromProto(proto.signal_strength()),
          base::UTF8ToUTF16(proto.mobile_provider())},
      GetChargingStateFromProto(proto.charging_state()),
      GetBatterySaverStateFromProto(proto.battery_mode()),
      proto.battery_percentage());
}

}  // namespace

PhoneStatusProcessor::PhoneStatusProcessor(
    DoNotDisturbController* do_not_disturb_controller,
    FeatureStatusProvider* feature_status_provider,
    MessageReceiver* message_receiver,
    FindMyDeviceController* find_my_device_controller,
    NotificationAccessManager* notification_access_manager,
    ScreenLockManager* screen_lock_manager,
    NotificationProcessor* notification_processor_,
    MultiDeviceSetupClient* multidevice_setup_client,
    MutablePhoneModel* phone_model)
    : do_not_disturb_controller_(do_not_disturb_controller),
      feature_status_provider_(feature_status_provider),
      message_receiver_(message_receiver),
      find_my_device_controller_(find_my_device_controller),
      notification_access_manager_(notification_access_manager),
      screen_lock_manager_(screen_lock_manager),
      notification_processor_(notification_processor_),
      multidevice_setup_client_(multidevice_setup_client),
      phone_model_(phone_model) {
  DCHECK(do_not_disturb_controller_);
  DCHECK(feature_status_provider_);
  DCHECK(message_receiver_);
  DCHECK(find_my_device_controller_);
  DCHECK(notification_access_manager_);
  DCHECK(notification_processor_);
  DCHECK(multidevice_setup_client_);
  DCHECK(phone_model_);

  message_receiver_->AddObserver(this);
  feature_status_provider_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);

  MaybeSetPhoneModelName(multidevice_setup_client_->GetHostStatus().second);
}

PhoneStatusProcessor::~PhoneStatusProcessor() {
  message_receiver_->RemoveObserver(this);
  feature_status_provider_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);
}

void PhoneStatusProcessor::ProcessReceivedNotifications(
    const RepeatedPtrField<proto::Notification>& notification_protos) {
  chromeos::multidevice_setup::mojom::FeatureState feature_state =
      multidevice_setup_client_->GetFeatureState(
          chromeos::multidevice_setup::mojom::Feature::kPhoneHubNotifications);
  if (feature_state !=
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser) {
    // Do not process any notifications if notifications are not enabled in
    // settings.
    return;
  }

  std::vector<proto::Notification> inline_replyable_protos;

  for (const auto& proto : notification_protos) {
    if (!features::IsPhoneHubCallNotificationEnabled() &&
        (proto.category() == proto::Notification::Category::
                                 Notification_Category_INCOMING_CALL ||
         proto.category() == proto::Notification::Category::
                                 Notification_Category_ONGOING_CALL ||
         proto.category() == proto::Notification::Category::
                                 Notification_Category_SCREEN_CALL)) {
      continue;
    }
    inline_replyable_protos.emplace_back(proto);
  }

  notification_processor_->AddNotifications(inline_replyable_protos);
}

void PhoneStatusProcessor::SetReceivedPhoneStatusModelStates(
    const proto::PhoneProperties& phone_properties) {
  phone_model_->SetPhoneStatusModel(CreatePhoneStatusModel(phone_properties));

  do_not_disturb_controller_->SetDoNotDisturbStateInternal(
      phone_properties.notification_mode() ==
          proto::NotificationMode::DO_NOT_DISTURB_ON,
      phone_properties.profile_type() != proto::ProfileType::WORK_PROFILE);

  notification_access_manager_->SetAccessStatusInternal(
      ComputeNotificationAccessState(phone_properties));

  if (screen_lock_manager_) {
    screen_lock_manager_->SetLockStatusInternal(
        ComputeScreenLockState(phone_properties));
  }

  find_my_device_controller_->SetPhoneRingingStatusInternal(
      ComputeFindMyDeviceStatus(phone_properties));
}

void PhoneStatusProcessor::MaybeSetPhoneModelName(
    const absl::optional<multidevice::RemoteDeviceRef>& remote_device) {
  if (!remote_device.has_value()) {
    phone_model_->SetPhoneName(absl::nullopt);
    return;
  }

  phone_model_->SetPhoneName(base::UTF8ToUTF16(remote_device->name()));
}

void PhoneStatusProcessor::OnFeatureStatusChanged() {
  // Reset phone model instance when but still keep the phone's name.
  if (feature_status_provider_->GetStatus() !=
      FeatureStatus::kEnabledAndConnected) {
    phone_model_->SetPhoneStatusModel(absl::nullopt);
    notification_processor_->ClearNotificationsAndPendingUpdates();
  }
}

void PhoneStatusProcessor::OnPhoneStatusSnapshotReceived(
    proto::PhoneStatusSnapshot phone_status_snapshot) {
  PA_LOG(INFO) << "Received snapshot from phone with Android version "
               << phone_status_snapshot.properties().android_version()
               << " and GmsCore version "
               << phone_status_snapshot.properties().gmscore_version();
  ProcessReceivedNotifications(phone_status_snapshot.notifications());
  SetReceivedPhoneStatusModelStates(phone_status_snapshot.properties());
}

void PhoneStatusProcessor::OnPhoneStatusUpdateReceived(
    proto::PhoneStatusUpdate phone_status_update) {
  ProcessReceivedNotifications(phone_status_update.updated_notifications());
  SetReceivedPhoneStatusModelStates(phone_status_update.properties());

  if (!phone_status_update.removed_notification_ids().empty()) {
    base::flat_set<int64_t> removed_notification_ids;
    for (auto& id : phone_status_update.removed_notification_ids()) {
      removed_notification_ids.emplace(id);
    }

    notification_processor_->RemoveNotifications(removed_notification_ids);
  }
}

void PhoneStatusProcessor::OnHostStatusChanged(
    const MultiDeviceSetupClient::HostStatusWithDevice&
        host_device_with_status) {
  MaybeSetPhoneModelName(host_device_with_status.second);
}

}  // namespace phonehub
}  // namespace ash
