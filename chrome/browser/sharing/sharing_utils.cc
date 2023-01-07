// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"

namespace {

bool CanListDevices(syncer::SyncService* sync_service) {
  syncer::ModelTypeSet active_data_types = sync_service->GetActiveDataTypes();

  // Can list device using DeviceInfo and sharing.synced_devices preferences.
  if (active_data_types.HasAll({syncer::DEVICE_INFO, syncer::PREFERENCES}))
    return true;

  // Can list device using only DeviceInfo.
  if (active_data_types.Has(syncer::DEVICE_INFO)) {
    return true;
  }

  return false;
}

}  // namespace

bool CanSendViaVapid(syncer::SyncService* sync_service) {
  // Can send using VAPID key in sharing.vapid_key preferences.
  return sync_service->GetActiveDataTypes().Has(syncer::PREFERENCES);
}

bool CanSendViaSenderID(syncer::SyncService* sync_service) {
  return base::FeatureList::IsEnabled(kSharingSendViaSync) &&
         sync_service->GetActiveDataTypes().Has(syncer::SHARING_MESSAGE);
}

bool IsSyncEnabledForSharing(syncer::SyncService* sync_service) {
  if (!sync_service)
    return false;

  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::ACTIVE) {
    return false;
  }

  if (!CanListDevices(sync_service)) {
    return false;
  }

  if (!CanSendViaVapid(sync_service) && !CanSendViaSenderID(sync_service)) {
    return false;
  }

  return true;
}

bool IsSyncDisabledForSharing(syncer::SyncService* sync_service) {
  // Sync service is not initialized, we can't be sure it's disabled.
  if (!sync_service)
    return false;

  if (sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::DISABLED ||
      sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::PAUSED) {
    return true;
  }

  // Ignore transient states.
  if (sync_service->GetTransportState() !=
      syncer::SyncService::TransportState::ACTIVE) {
    return false;
  }

  if (!CanListDevices(sync_service))
    return true;

  if (!CanSendViaVapid(sync_service) && !CanSendViaSenderID(sync_service))
    return true;

  return false;
}

absl::optional<chrome_browser_sharing::FCMChannelConfiguration> GetFCMChannel(
    const syncer::DeviceInfo& device_info) {
  if (!device_info.sharing_info())
    return absl::nullopt;

  chrome_browser_sharing::FCMChannelConfiguration fcm_configuration;
  auto& vapid_target_info = device_info.sharing_info()->vapid_target_info;
  auto& sender_id_target_info =
      device_info.sharing_info()->sender_id_target_info;
  fcm_configuration.set_vapid_fcm_token(vapid_target_info.fcm_token);
  fcm_configuration.set_vapid_p256dh(vapid_target_info.p256dh);
  fcm_configuration.set_vapid_auth_secret(vapid_target_info.auth_secret);
  fcm_configuration.set_sender_id_fcm_token(sender_id_target_info.fcm_token);
  fcm_configuration.set_sender_id_p256dh(sender_id_target_info.p256dh);
  fcm_configuration.set_sender_id_auth_secret(
      sender_id_target_info.auth_secret);

  return fcm_configuration;
}

SharingDevicePlatform GetDevicePlatform(const syncer::DeviceInfo& device_info) {
  switch (device_info.device_type()) {
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_CROS:
      return SharingDevicePlatform::kChromeOS;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_LINUX:
      return SharingDevicePlatform::kLinux;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_MAC:
      return SharingDevicePlatform::kMac;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_WIN:
      return SharingDevicePlatform::kWindows;
    case sync_pb::SyncEnums_DeviceType_TYPE_PHONE:
    case sync_pb::SyncEnums_DeviceType_TYPE_TABLET:
      if (device_info.manufacturer_name() == "Apple Inc.")
        return SharingDevicePlatform::kIOS;
      return SharingDevicePlatform::kAndroid;
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_UNSET:
    case sync_pb::SyncEnums::DeviceType::SyncEnums_DeviceType_TYPE_OTHER:
      return SharingDevicePlatform::kUnknown;
  }
}
