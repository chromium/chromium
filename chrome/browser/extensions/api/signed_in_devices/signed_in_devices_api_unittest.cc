// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_api.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DeviceInfo;
using syncer::FakeDeviceInfoSyncService;
using syncer::FakeDeviceInfoTracker;

namespace extensions {

TEST(SignedInDevicesAPITest, GetSignedInDevices) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeDeviceInfoTracker device_tracker;
  TestExtensionPrefs extension_prefs(base::ThreadTaskRunnerHandle::Get().get());

  // Add a couple of devices and make sure we get back public ids for them.
  std::string extension_name = "test";
  scoped_refptr<Extension> extension_test =
      extension_prefs.AddExtension(extension_name);

  DeviceInfo device_info1(base::GenerateGUID(), "abc Device", "XYZ v1",
                          "XYZ SyncAgent v1",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
                          base::SysInfo::HardwareInfo(), base::Time(),
                          /*send_tab_to_self_receiving_enabled=*/true,
                          /*sharing_info=*/base::nullopt);

  DeviceInfo device_info2(base::GenerateGUID(), "def Device", "XYZ v2",
                          "XYZ SyncAgent v2",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
                          base::SysInfo::HardwareInfo(), base::Time(),
                          /*send_tab_to_self_receiving_enabled=*/true,
                          /*sharing_info=*/base::nullopt);

  device_tracker.Add(&device_info1);
  device_tracker.Add(&device_info2);

  std::vector<std::unique_ptr<DeviceInfo>> output1 = GetAllSignedInDevices(
      extension_test->id(), &device_tracker, extension_prefs.prefs());

  std::string public_id1 = output1[0]->public_id();
  std::string public_id2 = output1[1]->public_id();

  EXPECT_FALSE(public_id1.empty());
  EXPECT_FALSE(public_id2.empty());
  EXPECT_NE(public_id1, public_id2);

  // Add a third device and make sure the first 2 ids are retained and a new
  // id is generated for the third device.
  DeviceInfo device_info3(base::GenerateGUID(), "def Device", "jkl v2",
                          "XYZ SyncAgent v2",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
                          base::SysInfo::HardwareInfo(), base::Time(),
                          /*send_tab_to_self_receiving_enabled=*/true,
                          /*sharing_info=*/base::nullopt);

  device_tracker.Add(&device_info3);

  std::vector<std::unique_ptr<DeviceInfo>> output2 = GetAllSignedInDevices(
      extension_test->id(), &device_tracker, extension_prefs.prefs());

  EXPECT_EQ(output2[0]->public_id(), public_id1);
  EXPECT_EQ(output2[1]->public_id(), public_id2);

  std::string public_id3 = output2[2]->public_id();
  EXPECT_FALSE(public_id3.empty());
  EXPECT_NE(public_id3, public_id1);
  EXPECT_NE(public_id3, public_id2);
}

std::unique_ptr<KeyedService> CreateFakeDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<FakeDeviceInfoSyncService>();
}

class ExtensionSignedInDevicesTest : public ExtensionApiUnittest {
 private:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{DeviceInfoSyncServiceFactory::GetInstance(),
             base::BindRepeating(&CreateFakeDeviceInfoSyncService)}};
  }
};

std::string GetPublicId(const base::DictionaryValue* dictionary) {
  std::string public_id;
  if (!dictionary->GetString("id", &public_id)) {
    ADD_FAILURE() << "Not able to find public id in the dictionary";
  }

  return public_id;
}

void VerifyDictionaryWithDeviceInfo(const base::DictionaryValue* actual_value,
                                    DeviceInfo* device_info) {
  std::string public_id = GetPublicId(actual_value);
  device_info->set_public_id(public_id);

  std::unique_ptr<base::DictionaryValue> expected_value(device_info->ToValue());
  EXPECT_TRUE(expected_value->Equals(actual_value));
}

base::DictionaryValue* GetDictionaryFromList(int index,
                                             base::ListValue* value) {
  base::DictionaryValue* dictionary;
  if (!value->GetDictionary(index, &dictionary)) {
    ADD_FAILURE() << "Expected a list of dictionaries";
    return NULL;
  }
  return dictionary;
}

TEST_F(ExtensionSignedInDevicesTest, GetAll) {
  FakeDeviceInfoTracker* device_tracker = static_cast<FakeDeviceInfoTracker*>(
      DeviceInfoSyncServiceFactory::GetForProfile(profile())
          ->GetDeviceInfoTracker());

  DeviceInfo device_info1(base::GenerateGUID(), "abc Device", "XYZ v1",
                          "XYZ SyncAgent v1",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
                          base::SysInfo::HardwareInfo(), base::Time(),
                          /*send_tab_to_self_receiving_enabled=*/true,
                          /*sharing_info=*/base::nullopt);

  DeviceInfo device_info2(base::GenerateGUID(), "def Device", "XYZ v2",
                          "XYZ SyncAgent v2",
                          sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id",
                          base::SysInfo::HardwareInfo(), base::Time(),
                          /*send_tab_to_self_receiving_enabled=*/true,
                          /*sharing_info=*/base::nullopt);

  device_tracker->Add(&device_info1);
  device_tracker->Add(&device_info2);

  std::unique_ptr<base::ListValue> result(
      RunFunctionAndReturnList(new SignedInDevicesGetFunction(), "[null]"));

  // Ensure dictionary matches device info.
  VerifyDictionaryWithDeviceInfo(GetDictionaryFromList(0, result.get()),
                                 &device_info1);
  VerifyDictionaryWithDeviceInfo(GetDictionaryFromList(1, result.get()),
                                 &device_info2);

  // Ensure public ids are set and unique.
  std::string public_id1 = GetPublicId(GetDictionaryFromList(0, result.get()));
  std::string public_id2 = GetPublicId(GetDictionaryFromList(1, result.get()));

  EXPECT_FALSE(public_id1.empty());
  EXPECT_FALSE(public_id2.empty());
  EXPECT_NE(public_id1, public_id2);
}

TEST_F(ExtensionSignedInDevicesTest, DeviceInfoTrackerNotInitialized) {
  std::vector<std::unique_ptr<DeviceInfo>> output =
      GetAllSignedInDevices(extension()->id(), profile());

  EXPECT_TRUE(output.empty());
}

}  // namespace extensions
