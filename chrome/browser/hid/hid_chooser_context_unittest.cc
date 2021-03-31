// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_chooser_context.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/mock_hid_device_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/test/chooser_context_base_mock_permission_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunClosure;
using ::testing::_;

namespace {

// Main text fixture.
class HidChooserContextTest : public testing::Test {
 public:
  HidChooserContextTest()
      : origin_(url::Origin::Create(GURL("https://google.com"))) {}
  HidChooserContextTest(const HidChooserContextTest&) = delete;
  HidChooserContextTest& operator=(const HidChooserContextTest&) = delete;
  ~HidChooserContextTest() override = default;

  const url::Origin& origin() { return origin_; }
  Profile* profile() { return &profile_; }
  permissions::MockPermissionObserver& permission_observer() {
    return mock_permission_observer_;
  }
  MockHidDeviceObserver& device_observer() { return mock_device_observer_; }

  HidChooserContext* GetContext() {
    auto* context = HidChooserContextFactory::GetForProfile(&profile_);
    if (!observers_added_) {
      context->AddObserver(&mock_permission_observer_);
      context->AddDeviceObserver(&mock_device_observer_);
      observers_added_ = true;
    }
    return context;
  }

  void SetUp() override {
    mojo::PendingRemote<device::mojom::HidManager> hid_manager;
    hid_manager_.Bind(hid_manager.InitWithNewPipeAndPassReceiver());
    auto* chooser_context = HidChooserContextFactory::GetForProfile(&profile_);

    // Connect the HidManager and ensure we've received the initial enumeration
    // before continuing.
    base::RunLoop run_loop;
    chooser_context->SetHidManagerForTesting(
        std::move(hid_manager),
        base::BindLambdaForTesting(
            [&run_loop](std::vector<device::mojom::HidDeviceInfoPtr> devices) {
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void TearDown() override {
    if (observers_added_) {
      auto* context = HidChooserContextFactory::GetForProfile(&profile_);
      context->RemoveObserver(&mock_permission_observer_);
      context->RemoveDeviceObserver(&mock_device_observer_);
    }
  }

  device::mojom::HidDeviceInfoPtr ConnectEphemeralDevice() {
    return hid_manager_.CreateAndAddDevice(
        "physical-device-id", 0x1234, 0xabcd, "product-name",
        /*serial_number=*/"", device::mojom::HidBusType::kHIDBusTypeUSB);
  }

  device::mojom::HidDeviceInfoPtr ConnectPersistentUsbDevice() {
    return hid_manager_.CreateAndAddDevice(
        "physical-device-id", 0x1234, 0xabcd, "product-name", "serial-number",
        device::mojom::HidBusType::kHIDBusTypeUSB);
  }

  void ConnectDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.AddDevice(device.Clone());
  }

  void DisconnectDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.RemoveDevice(device.guid);
  }

  void UpdateDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.ChangeDevice(device.Clone());
  }

  void SimulateHidManagerConnectionError() {
    hid_manager_.SimulateConnectionError();
  }

 private:
  url::Origin origin_;
  device::FakeHidManager hid_manager_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  permissions::MockPermissionObserver mock_permission_observer_;
  MockHidDeviceObserver mock_device_observer_;
  bool observers_added_ = false;
};

}  // namespace

TEST_F(HidChooserContextTest, GrantAndRevokeEphemeralDevice) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop permission_granted_loop;
  EXPECT_CALL(permission_observer(), OnChooserObjectPermissionChanged(
                                         ContentSettingsType::HID_GUARD,
                                         ContentSettingsType::HID_CHOOSER_DATA))
      .WillOnce(RunClosure(permission_granted_loop.QuitClosure()))
      .WillOnce([]() {
        // Expect a 2nd permission change event when the permission is revoked.
      });

  base::RunLoop permission_revoked_loop;
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin()))
      .WillOnce(RunClosure(permission_revoked_loop.QuitClosure()));

  HidChooserContext* context = GetContext();

  // 1. Connect a device that is only eligible for ephemeral permissions.
  auto device = ConnectEphemeralDevice();
  device_added_loop.Run();

  EXPECT_FALSE(context->HasDevicePermission(origin(), *device));

  // 2. Grant an ephemeral permission.
  context->GrantDevicePermission(origin(), *device);
  permission_granted_loop.Run();

  EXPECT_TRUE(context->HasDevicePermission(origin(), *device));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context->GetGrantedObjects(origin());
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin().GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  // 3. Revoke the permission.
  context->RevokeObjectPermission(origin(), objects[0]->value);
  permission_revoked_loop.Run();

  EXPECT_FALSE(context->HasDevicePermission(origin(), *device));
  origin_objects = context->GetGrantedObjects(origin());
  EXPECT_EQ(0u, origin_objects.size());
  objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(HidChooserContextTest, GrantAndDisconnectEphemeralDevice) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  EXPECT_CALL(device_observer(), OnDeviceRemoved(_));

  base::RunLoop permission_granted_loop;
  EXPECT_CALL(permission_observer(), OnChooserObjectPermissionChanged(
                                         ContentSettingsType::HID_GUARD,
                                         ContentSettingsType::HID_CHOOSER_DATA))
      .WillOnce(RunClosure(permission_granted_loop.QuitClosure()))
      .WillOnce([]() {
        // Expect a 2nd permission change event when the permission is revoked.
      });

  base::RunLoop permission_revoked_loop;
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin()))
      .WillOnce(RunClosure(permission_revoked_loop.QuitClosure()));

  HidChooserContext* context = GetContext();

  // 1. Connect a device that is only eligible for ephemeral permissions.
  auto device = ConnectEphemeralDevice();
  device_added_loop.Run();

  EXPECT_FALSE(context->HasDevicePermission(origin(), *device));

  // 2. Grant an ephemeral permission.
  context->GrantDevicePermission(origin(), *device);
  permission_granted_loop.Run();

  EXPECT_TRUE(context->HasDevicePermission(origin(), *device));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context->GetGrantedObjects(origin());
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin().GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  // 3. Disconnect the device. Because an ephemeral permission was granted, the
  // permission should be revoked on disconnect.
  DisconnectDevice(*device);
  permission_revoked_loop.Run();

  EXPECT_FALSE(context->HasDevicePermission(origin(), *device));
  origin_objects = context->GetGrantedObjects(origin());
  EXPECT_EQ(0u, origin_objects.size());
  objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(HidChooserContextTest, GrantDisconnectRevokeUsbPersistentDevice) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop device_removed_loop;
  EXPECT_CALL(device_observer(), OnDeviceRemoved(_))
      .WillOnce(RunClosure(device_removed_loop.QuitClosure()));

  base::RunLoop permission_granted_loop;
  EXPECT_CALL(permission_observer(), OnChooserObjectPermissionChanged(
                                         ContentSettingsType::HID_GUARD,
                                         ContentSettingsType::HID_CHOOSER_DATA))
      .WillOnce(RunClosure(permission_granted_loop.QuitClosure()))
      .WillOnce([]() {
        // Expect a 2nd permission change event when the permission is revoked.
      });

  base::RunLoop permission_revoked_loop;
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin()))
      .WillOnce(RunClosure(permission_revoked_loop.QuitClosure()));

  HidChooserContext* context = GetContext();

  // 1. Connect a USB device eligible for persistent permissions.
  auto device = ConnectPersistentUsbDevice();
  device_added_loop.Run();

  EXPECT_FALSE(context->HasDevicePermission(origin(), *device));

  // 2. Grant a persistent permission.
  context->GrantDevicePermission(origin(), *device);
  permission_granted_loop.Run();

  EXPECT_TRUE(context->HasDevicePermission(origin(), *device));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context->GetGrantedObjects(origin());
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin().GetURL(), objects[0]->origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  // 3. Disconnect the device. The permission should not be revoked.
  DisconnectDevice(*device);
  device_removed_loop.Run();

  EXPECT_TRUE(context->HasDevicePermission(origin(), *device));

  // 4. Revoke the persistent permission.
  context->RevokeObjectPermission(origin(), objects[0]->value);
  permission_revoked_loop.Run();

  EXPECT_FALSE(context->HasDevicePermission(origin(), *device));
  origin_objects = context->GetGrantedObjects(origin());
  EXPECT_EQ(0u, origin_objects.size());
  objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(HidChooserContextTest, GuardPermission) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop permission_granted_loop;
  EXPECT_CALL(permission_observer(), OnChooserObjectPermissionChanged(
                                         ContentSettingsType::HID_GUARD,
                                         ContentSettingsType::HID_CHOOSER_DATA))
      .WillOnce(RunClosure(permission_granted_loop.QuitClosure()));

  HidChooserContext* context = GetContext();

  // 1. Connect a device that is only eligible for ephemeral permissions.
  auto device = ConnectEphemeralDevice();
  device_added_loop.Run();

  // 2. Grant an ephemeral device permission.
  context->GrantDevicePermission(origin(), *device);
  permission_granted_loop.Run();

  EXPECT_TRUE(context->HasDevicePermission(origin(), *device));

  // 3. Set the guard permission to CONTENT_SETTING_BLOCK.
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(origin().GetURL(), origin().GetURL(),
                                     ContentSettingsType::HID_GUARD,
                                     CONTENT_SETTING_BLOCK);

  // 4. Check that the device permission is no longer granted.
  EXPECT_FALSE(context->HasDevicePermission(origin(), *device));

  auto objects = context->GetGrantedObjects(origin());
  EXPECT_EQ(0u, objects.size());

  auto all_origin_objects = context->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(HidChooserContextTest, ConnectionErrorWithEphemeralPermission) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  EXPECT_CALL(device_observer(), OnHidManagerConnectionError());

  base::RunLoop permission_granted_loop;
  EXPECT_CALL(permission_observer(), OnChooserObjectPermissionChanged(
                                         ContentSettingsType::HID_GUARD,
                                         ContentSettingsType::HID_CHOOSER_DATA))
      .WillOnce(RunClosure(permission_granted_loop.QuitClosure()))
      .WillOnce([]() {
        // Expect a 2nd permission change event when the permission is revoked.
      });

  base::RunLoop permission_revoked_loop;
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin()))
      .WillOnce(RunClosure(permission_revoked_loop.QuitClosure()));

  HidChooserContext* context = GetContext();

  // 1. Connect a device that is only eligible for persistent permissions.
  auto device = ConnectEphemeralDevice();
  device_added_loop.Run();

  // 2. Grant an ephemeral device permission.
  context->GrantDevicePermission(origin(), *device);
  permission_granted_loop.Run();

  // 3. Simulate a connection error. The ephemeral permission should be revoked.
  SimulateHidManagerConnectionError();
  permission_revoked_loop.Run();

  EXPECT_FALSE(context->HasDevicePermission(origin(), *device));
}

TEST_F(HidChooserContextTest, ConnectionErrorWithPersistentPermission) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop connection_error_loop;
  EXPECT_CALL(device_observer(), OnHidManagerConnectionError())
      .WillOnce(RunClosure(connection_error_loop.QuitClosure()));

  base::RunLoop permission_granted_loop;
  EXPECT_CALL(permission_observer(), OnChooserObjectPermissionChanged(
                                         ContentSettingsType::HID_GUARD,
                                         ContentSettingsType::HID_CHOOSER_DATA))
      .WillOnce(RunClosure(permission_granted_loop.QuitClosure()))
      .WillOnce([]() {
        // Expect a 2nd permission change event when the permission is revoked.
      });

  HidChooserContext* context = GetContext();

  // 1. Connect a device that is only eligible for persistent permissions.
  auto device = ConnectPersistentUsbDevice();
  device_added_loop.Run();

  // 2. Grant a persistent device permission.
  context->GrantDevicePermission(origin(), *device);
  permission_granted_loop.Run();

  // 3. Simulate a connection error. The persistent permission should not be
  // affected.
  SimulateHidManagerConnectionError();
  connection_error_loop.Run();

  EXPECT_TRUE(context->HasDevicePermission(origin(), *device));
}

namespace {

device::mojom::HidDeviceInfoPtr CreateDeviceWithOneCollection(
    const std::string& guid) {
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = guid;
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(1, 1);
  collection->input_reports.push_back(
      device::mojom::HidReportDescription::New());
  device_info->collections.push_back(std::move(collection));
  return device_info;
}

device::mojom::HidDeviceInfoPtr CreateDeviceWithTwoCollections(
    const std::string& guid) {
  auto device_info = CreateDeviceWithOneCollection(guid);
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(2, 2);
  collection->output_reports.push_back(
      device::mojom::HidReportDescription::New());
  device_info->collections.push_back(std::move(collection));
  return device_info;
}

}  // namespace

TEST_F(HidChooserContextTest, AddChangeRemoveDevice) {
  const char kTestGuid[] = "guid";

  HidChooserContext* context = GetContext();

  EXPECT_FALSE(context->GetDeviceInfo(kTestGuid));

  // Connect a partially-initialized device.
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 1u);
    device_added_loop.Quit();
  });
  auto partial_device = CreateDeviceWithOneCollection(kTestGuid);
  ConnectDevice(*partial_device);
  device_added_loop.Run();

  auto* device_info = context->GetDeviceInfo(kTestGuid);
  ASSERT_TRUE(device_info);
  EXPECT_EQ(device_info->collections.size(), 1u);

  // Update the device to add another collection.
  base::RunLoop device_changed_loop;
  EXPECT_CALL(device_observer(), OnDeviceChanged).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 2u);
    device_changed_loop.Quit();
  });
  auto complete_device = CreateDeviceWithTwoCollections(kTestGuid);
  UpdateDevice(*complete_device);
  device_changed_loop.Run();

  device_info = context->GetDeviceInfo(kTestGuid);
  ASSERT_TRUE(device_info);
  EXPECT_EQ(device_info->collections.size(), 2u);

  // Disconnect the device.
  base::RunLoop device_removed_loop;
  EXPECT_CALL(device_observer(), OnDeviceRemoved).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 2u);
    device_removed_loop.Quit();
  });
  DisconnectDevice(*complete_device);
  device_removed_loop.Run();

  ASSERT_FALSE(context->GetDeviceInfo(kTestGuid));
}
