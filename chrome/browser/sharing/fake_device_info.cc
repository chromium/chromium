// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/fake_device_info.h"

#include "components/sync_device_info/device_info_util.h"

std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& guid,
    const std::string& name,
    const base::Optional<syncer::DeviceInfo::SharingInfo>& sharing_info,
    sync_pb::SyncEnums_DeviceType device_type,
    const std::string& manufacturer_name,
    const std::string& model_name,
    base::Time last_updated_timestamp) {
  return std::make_unique<syncer::DeviceInfo>(
      guid, name, "chrome_version", "user_agent", device_type, "device_id",
      manufacturer_name, model_name, last_updated_timestamp,
      syncer::DeviceInfoUtil::GetPulseInterval(),
      /*send_tab_to_self_receiving_enabled=*/false, sharing_info,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::ModelTypeSet());
}
