// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_FAKE_DEVICE_INFO_H_
#define CHROME_BROWSER_SHARING_FAKE_DEVICE_INFO_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "components/sync_device_info/device_info.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer

std::unique_ptr<syncer::DeviceInfo> CreateFakeDeviceInfo(
    const std::string& guid,
    const std::string& name = "name",
    const base::Optional<syncer::DeviceInfo::SharingInfo>& sharing_info =
        base::nullopt,
    sync_pb::SyncEnums_DeviceType device_type =
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
    const std::string& manufacturer_name = "manufacturer",
    const std::string& model_name = "model",
    const std::string& full_hardware_class = std::string(),
    base::Time last_updated_timestamp = base::Time::Now());

#endif  // CHROME_BROWSER_SHARING_FAKE_DEVICE_INFO_H_
