// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_utils.h"

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Util function to return a string denoting the type of device.
std::string GetDeviceType(sync_pb::SyncEnums::DeviceType type) {
  int device_type_message_id = -1;

  switch (type) {
    case sync_pb::SyncEnums::TYPE_LINUX:
    case sync_pb::SyncEnums::TYPE_WIN:
    case sync_pb::SyncEnums::TYPE_CROS:
    case sync_pb::SyncEnums::TYPE_MAC:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_COMPUTER;
      break;

    case sync_pb::SyncEnums::TYPE_UNSET:
    case sync_pb::SyncEnums::TYPE_OTHER:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_DEVICE;
      break;

    case sync_pb::SyncEnums::TYPE_PHONE:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_PHONE;
      break;

    case sync_pb::SyncEnums::TYPE_TABLET:
      device_type_message_id = IDS_BROWSER_SHARING_DEVICE_TYPE_TABLET;
      break;
  }

  return l10n_util::GetStringUTF8(device_type_message_id);
}

std::string CapitalizeWords(const std::string& sentence) {
  std::string capitalized_sentence;
  bool use_upper_case = true;
  for (char ch : sentence) {
    capitalized_sentence += (use_upper_case ? toupper(ch) : ch);
    use_upper_case = !isalpha(ch);
  }
  return capitalized_sentence;
}

syncer::ModelTypeSet GetRequiredSyncDataTypes() {
  // DeviceInfo is always required to list devices.
  syncer::ModelTypeSet required_data_types(syncer::DEVICE_INFO);

  // Legacy implementation of device list and VAPID key uses sync preferences.
  if (!base::FeatureList::IsEnabled(kSharingUseDeviceInfo) ||
      !base::FeatureList::IsEnabled(kSharingDeriveVapidKey)) {
    required_data_types.Put(syncer::PREFERENCES);
  }

  return required_data_types;
}

}  // namespace

SharingDeviceNames GetSharingDeviceNames(const syncer::DeviceInfo* device) {
  DCHECK(device);
  SharingDeviceNames device_names;

  base::SysInfo::HardwareInfo hardware_info = device->hardware_info();
  sync_pb::SyncEnums::DeviceType type = device->device_type();
  // 1. Skip renaming for M78- devices where HardwareInfo is not available.
  // 2. Skip renaming if client_name is high quality i.e. not equals to model.
  // 3. Skip renaming for Android and Chrome OS devices if feature is not
  //    enabled, which always have low quality client_name.
  if (hardware_info.model.empty() ||
      hardware_info.model != device->client_name() ||
      (!base::FeatureList::IsEnabled(kSharingRenameDevices) &&
       (type == sync_pb::SyncEnums::TYPE_CROS ||
        type == sync_pb::SyncEnums::TYPE_PHONE ||
        type == sync_pb::SyncEnums::TYPE_TABLET))) {
    device_names.full_name = device_names.short_name = device->client_name();
    return device_names;
  }

  hardware_info.manufacturer = CapitalizeWords(hardware_info.manufacturer);

  // For chromeOS, return manufacturer + model.
  if (type == sync_pb::SyncEnums::TYPE_CROS) {
    device_names.short_name = device_names.full_name =
        base::StrCat({hardware_info.manufacturer, " ", hardware_info.model});
    return device_names;
  }

  if (hardware_info.manufacturer == "Apple Inc.") {
    // Internal names of Apple devices are formatted as MacbookPro2,3 or
    // iPhone2,1 or Ipad4,1.
    device_names.short_name = hardware_info.model.substr(
        0, hardware_info.model.find_first_of("0123456789,"));
    device_names.full_name = hardware_info.model;
    return device_names;
  }

  device_names.short_name =
      base::StrCat({hardware_info.manufacturer, " ", GetDeviceType(type)});
  device_names.full_name =
      base::StrCat({device_names.short_name, " ", hardware_info.model});
  return device_names;
}

bool IsSyncEnabledForSharing(syncer::SyncService* sync_service) {
  if (!sync_service)
    return false;

  bool is_sync_enabled =
      sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::ACTIVE &&
      sync_service->GetActiveDataTypes().HasAll(GetRequiredSyncDataTypes());
  // TODO(crbug.com/1012226): Remove local sync check when we have dedicated
  // Sharing data type.
  if (base::FeatureList::IsEnabled(kSharingDeriveVapidKey))
    is_sync_enabled &= !sync_service->IsLocalSyncEnabled();
  return is_sync_enabled;
}

bool IsSyncDisabledForSharing(syncer::SyncService* sync_service) {
  if (!sync_service)
    return false;

  bool is_sync_disabled =
      sync_service->GetTransportState() ==
          syncer::SyncService::TransportState::DISABLED ||
      (sync_service->GetTransportState() ==
           syncer::SyncService::TransportState::ACTIVE &&
       !sync_service->GetActiveDataTypes().HasAll(GetRequiredSyncDataTypes()));
  // TODO(crbug.com/1012226): Remove local sync check when we have dedicated
  // Sharing data type.
  if (base::FeatureList::IsEnabled(kSharingDeriveVapidKey))
    is_sync_disabled |= sync_service->IsLocalSyncEnabled();
  return is_sync_disabled;
}
