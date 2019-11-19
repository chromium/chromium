// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/signed_in_devices/id_mapping_helper.h"

#include <memory>
#include <string>

#include "base/guid.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DeviceInfo;

namespace extensions {
bool VerifyDictionary(const std::string& path,
                      const std::string& expected_value,
                      const base::DictionaryValue& dictionary) {
  std::string out;
  if (dictionary.GetString(path, &out)) {
    return (out == expected_value);
  }

  return false;
}

TEST(IdMappingHelperTest, SetIdsForDevices) {
  std::vector<std::unique_ptr<DeviceInfo>> devices;

  devices.push_back(std::make_unique<DeviceInfo>(
      base::GenerateGUID(), "abc Device", "XYZ v1", "XYZ SyncAgent v1",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id1",
      base::SysInfo::HardwareInfo(), base::Time(),
      /*send_tab_to_self_receiving_enabled=*/true,
      /*sharing_info=*/base::nullopt));

  devices.push_back(std::make_unique<DeviceInfo>(
      base::GenerateGUID(), "def Device", "XYZ v1", "XYZ SyncAgent v1",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id2",
      base::SysInfo::HardwareInfo(), base::Time(),
      /*send_tab_to_self_receiving_enabled=*/true,
      /*sharing_info=*/base::nullopt));

  base::DictionaryValue dictionary;

  CreateMappingForUnmappedDevices(devices, &dictionary);

  std::string public_id1 = devices[0]->public_id();
  std::string public_id2 = devices[1]->public_id();

  EXPECT_FALSE(public_id1.empty());
  EXPECT_FALSE(public_id2.empty());

  EXPECT_NE(public_id1, public_id2);

  // Now add a third device.
  devices.push_back(std::make_unique<DeviceInfo>(
      base::GenerateGUID(), "ghi Device", "XYZ v1", "XYZ SyncAgent v1",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id3",
      base::SysInfo::HardwareInfo(), base::Time(),
      /*send_tab_to_self_receiving_enabled=*/true,
      /*sharing_info=*/base::nullopt));

  CreateMappingForUnmappedDevices(devices, &dictionary);

  // Now make sure the existing ids are not changed.
  EXPECT_EQ(public_id1, devices[0]->public_id());
  EXPECT_EQ(public_id2, devices[1]->public_id());

  // Now make sure the id for third device is non empty and different.
  std::string public_id3 = devices[2]->public_id();
  EXPECT_FALSE(public_id3.empty());
  EXPECT_NE(public_id3, public_id1);
  EXPECT_NE(public_id3, public_id2);

  // Verify the dictionary.
  EXPECT_TRUE(VerifyDictionary(public_id1, devices[0]->guid(), dictionary));
  EXPECT_TRUE(VerifyDictionary(public_id2, devices[1]->guid(), dictionary));
  EXPECT_TRUE(VerifyDictionary(public_id3, devices[2]->guid(), dictionary));

  EXPECT_EQ(dictionary.size(), 3U);
}
}  // namespace extensions
