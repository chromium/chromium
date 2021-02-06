// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using device::FakeUsbDeviceInfo;
using device::mojom::HidBusType;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

}  // namespace

class DevicePermissionsManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    env_.reset(new extensions::TestExtensionEnvironment());
    extension_ = env_->MakeExtension(*base::test::ParseJsonDeprecated(
        "{"
        "  \"app\": {"
        "    \"background\": {"
        "      \"scripts\": [\"background.js\"]"
        "    }"
        "  },"
        "  \"permissions\": [ \"hid\", \"usb\" ]"
        "}"));

    // Set fake device manager for extensions::UsbDeviceManager.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
    fake_usb_manager_.AddReceiver(usb_manager.InitWithNewPipeAndPassReceiver());
    UsbDeviceManager::Get(env_->profile())
        ->SetDeviceManagerForTesting(std::move(usb_manager));
    base::RunLoop().RunUntilIdle();

    device0_ = fake_usb_manager_.CreateAndAddDevice(0, 0, "Test Manufacturer",
                                                    "Test Product", "ABCDE");
    device1_ = fake_usb_manager_.CreateAndAddDevice(0, 0, "Test Manufacturer",
                                                    "Test Product", "");
    device2_ = fake_usb_manager_.CreateAndAddDevice(0, 0, "Test Manufacturer",
                                                    "Test Product", "12345");
    device3_ = fake_usb_manager_.CreateAndAddDevice(0, 0, "Test Manufacturer",
                                                    "Test Product", "");

    HidDeviceManager::OverrideHidManagerBinderForTesting(base::BindRepeating(
        &device::FakeHidManager::Bind, base::Unretained(&fake_hid_manager_)));
    HidDeviceManager::Get(env_->profile())->LazyInitialize();
    base::RunLoop().RunUntilIdle();

    device4_ = fake_hid_manager_.CreateAndAddDevice(
        "4", 0, 0, "Test HID Device", "abcde", HidBusType::kHIDBusTypeUSB);
    device5_ = fake_hid_manager_.CreateAndAddDevice(
        "5", 0, 0, "Test HID Device", "", HidBusType::kHIDBusTypeUSB);
    device6_ = fake_hid_manager_.CreateAndAddDevice(
        "6", 0, 0, "Test HID Device", "67890", HidBusType::kHIDBusTypeUSB);
    device7_ = fake_hid_manager_.CreateAndAddDevice(
        "7", 0, 0, "Test HID Device", "", HidBusType::kHIDBusTypeUSB);
  }

  void TearDown() override { env_.reset(nullptr); }

  std::unique_ptr<extensions::TestExtensionEnvironment> env_;
  const extensions::Extension* extension_;
  device::FakeUsbDeviceManager fake_usb_manager_;
  device::mojom::UsbDeviceInfoPtr device0_;
  device::mojom::UsbDeviceInfoPtr device1_;
  device::mojom::UsbDeviceInfoPtr device2_;
  device::mojom::UsbDeviceInfoPtr device3_;

  device::FakeHidManager fake_hid_manager_;
  device::mojom::HidDeviceInfoPtr device4_;
  device::mojom::HidDeviceInfoPtr device5_;
  device::mojom::HidDeviceInfoPtr device6_;
  device::mojom::HidDeviceInfoPtr device7_;
};

TEST_F(DevicePermissionsManagerTest, AllowAndClearDevices) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), *device0_);
  manager->AllowUsbDevice(extension_->id(), *device1_);
  manager->AllowHidDevice(extension_->id(), *device4_);
  manager->AllowHidDevice(extension_->id(), *device5_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindUsbDeviceEntry(*device0_);
  ASSERT_TRUE(device0_entry);
  scoped_refptr<DevicePermissionEntry> device1_entry =
      device_permissions->FindUsbDeviceEntry(*device1_);
  ASSERT_TRUE(device1_entry);
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device2_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device3_));
  scoped_refptr<DevicePermissionEntry> device4_entry =
      device_permissions->FindHidDeviceEntry(*device4_);
  ASSERT_TRUE(device4_entry);
  scoped_refptr<DevicePermissionEntry> device5_entry =
      device_permissions->FindHidDeviceEntry(*device5_);
  ASSERT_TRUE(device5_entry);
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device6_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device7_));
  EXPECT_EQ(4U, device_permissions->entries().size());

  EXPECT_EQ(base::ASCIIToUTF16(
                "Test Product from Test Manufacturer (serial number ABCDE)"),
            device0_entry->GetPermissionMessageString());
  EXPECT_EQ(base::ASCIIToUTF16("Test Product from Test Manufacturer"),
            device1_entry->GetPermissionMessageString());
  EXPECT_EQ(base::ASCIIToUTF16("Test HID Device (serial number abcde)"),
            device4_entry->GetPermissionMessageString());
  EXPECT_EQ(base::ASCIIToUTF16("Test HID Device"),
            device5_entry->GetPermissionMessageString());

  manager->Clear(extension_->id());
  // The device_permissions object is deleted by Clear.
  device_permissions = manager->GetForExtension(extension_->id());

  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device0_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device1_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device2_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device3_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device4_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device5_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device6_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device7_));
  EXPECT_EQ(0U, device_permissions->entries().size());

  // After clearing device it should be possible to grant permission again.
  manager->AllowUsbDevice(extension_->id(), *device0_);
  manager->AllowUsbDevice(extension_->id(), *device1_);
  manager->AllowHidDevice(extension_->id(), *device4_);
  manager->AllowHidDevice(extension_->id(), *device5_);

  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device0_));
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device1_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device2_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device3_));
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device4_));
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device5_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device6_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device7_));
}

TEST_F(DevicePermissionsManagerTest, DisconnectDevice) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), *device0_);
  manager->AllowUsbDevice(extension_->id(), *device1_);
  manager->AllowHidDevice(extension_->id(), *device4_);
  manager->AllowHidDevice(extension_->id(), *device5_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device0_));
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device1_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device2_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device3_));
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device4_));
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device5_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device6_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device7_));

  fake_usb_manager_.RemoveDevice(device0_->guid);
  fake_usb_manager_.RemoveDevice(device1_->guid);

  fake_hid_manager_.RemoveDevice(device4_->guid);
  fake_hid_manager_.RemoveDevice(device5_->guid);

  base::RunLoop().RunUntilIdle();

  // Device 0 will be accessible when it is reconnected because it can be
  // recognized by its serial number.
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device0_));
  // Device 1 does not have a serial number and cannot be distinguished from
  // any other device of the same model so the app must request permission again
  // when it is reconnected.
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device1_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device2_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device3_));
  // Device 4 is like device 0, but HID.
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device4_));
  // Device 5 is like device 1, but HID.
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device5_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device6_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device7_));
}

TEST_F(DevicePermissionsManagerTest, RevokeAndRegrantAccess) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), *device0_);
  manager->AllowUsbDevice(extension_->id(), *device1_);
  manager->AllowHidDevice(extension_->id(), *device4_);
  manager->AllowHidDevice(extension_->id(), *device5_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindUsbDeviceEntry(*device0_);
  ASSERT_TRUE(device0_entry);
  scoped_refptr<DevicePermissionEntry> device1_entry =
      device_permissions->FindUsbDeviceEntry(*device1_);
  ASSERT_TRUE(device1_entry);
  scoped_refptr<DevicePermissionEntry> device4_entry =
      device_permissions->FindHidDeviceEntry(*device4_);
  ASSERT_TRUE(device4_entry);
  scoped_refptr<DevicePermissionEntry> device5_entry =
      device_permissions->FindHidDeviceEntry(*device5_);
  ASSERT_TRUE(device5_entry);

  manager->RemoveEntry(extension_->id(), device0_entry);
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device0_));
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device1_));

  manager->AllowUsbDevice(extension_->id(), *device0_);
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device0_));
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device1_));

  manager->RemoveEntry(extension_->id(), device1_entry);
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device0_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device1_));

  manager->AllowUsbDevice(extension_->id(), *device1_);
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device0_));
  EXPECT_TRUE(device_permissions->FindUsbDeviceEntry(*device1_));

  manager->RemoveEntry(extension_->id(), device4_entry);
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device4_));
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device5_));

  manager->AllowHidDevice(extension_->id(), *device4_);
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device4_));
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device5_));

  manager->RemoveEntry(extension_->id(), device5_entry);
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device4_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device5_));

  manager->AllowHidDevice(extension_->id(), *device5_);
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device4_));
  EXPECT_TRUE(device_permissions->FindHidDeviceEntry(*device5_));
}

TEST_F(DevicePermissionsManagerTest, UpdateLastUsed) {
  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  manager->AllowUsbDevice(extension_->id(), *device0_);
  manager->AllowHidDevice(extension_->id(), *device4_);

  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindUsbDeviceEntry(*device0_);
  EXPECT_TRUE(device0_entry->last_used().is_null());
  scoped_refptr<DevicePermissionEntry> device4_entry =
      device_permissions->FindHidDeviceEntry(*device4_);
  EXPECT_TRUE(device4_entry->last_used().is_null());

  manager->UpdateLastUsed(extension_->id(), device0_entry);
  EXPECT_FALSE(device0_entry->last_used().is_null());
  manager->UpdateLastUsed(extension_->id(), device4_entry);
  EXPECT_FALSE(device4_entry->last_used().is_null());
}

TEST_F(DevicePermissionsManagerTest, LoadPrefs) {
  std::unique_ptr<base::Value> prefs_value = base::test::ParseJsonDeprecated(
      "["
      "  {"
      "    \"manufacturer_string\": \"Test Manufacturer\","
      "    \"product_id\": 0,"
      "    \"product_string\": \"Test Product\","
      "    \"serial_number\": \"ABCDE\","
      "    \"type\": \"usb\","
      "    \"vendor_id\": 0"
      "  },"
      "  {"
      "    \"product_id\": 0,"
      "    \"product_string\": \"Test HID Device\","
      "    \"serial_number\": \"abcde\","
      "    \"type\": \"hid\","
      "    \"vendor_id\": 0"
      "  }"
      "]");
  env_->GetExtensionPrefs()->UpdateExtensionPref(extension_->id(), "devices",
                                                 std::move(prefs_value));

  DevicePermissionsManager* manager =
      DevicePermissionsManager::Get(env_->profile());
  DevicePermissions* device_permissions =
      manager->GetForExtension(extension_->id());
  scoped_refptr<DevicePermissionEntry> device0_entry =
      device_permissions->FindUsbDeviceEntry(*device0_);
  ASSERT_TRUE(device0_entry);
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device1_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device2_));
  EXPECT_FALSE(device_permissions->FindUsbDeviceEntry(*device3_));
  scoped_refptr<DevicePermissionEntry> device4_entry =
      device_permissions->FindHidDeviceEntry(*device4_);
  ASSERT_TRUE(device4_entry);
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device5_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device6_));
  EXPECT_FALSE(device_permissions->FindHidDeviceEntry(*device7_));

  EXPECT_EQ(base::ASCIIToUTF16(
                "Test Product from Test Manufacturer (serial number ABCDE)"),
            device0_entry->GetPermissionMessageString());
  EXPECT_EQ(base::ASCIIToUTF16("Test HID Device (serial number abcde)"),
            device4_entry->GetPermissionMessageString());
}

TEST_F(DevicePermissionsManagerTest, PermissionMessages) {
  base::string16 empty;
  base::string16 product(base::ASCIIToUTF16("Widget"));
  base::string16 manufacturer(base::ASCIIToUTF16("ACME"));
  base::string16 serial_number(base::ASCIIToUTF16("A"));

  EXPECT_EQ(base::ASCIIToUTF16("Unknown product 0001 from vendor 0000"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0000, 0x0001, empty, empty, empty, false));

  EXPECT_EQ(base::ASCIIToUTF16(
                "Unknown product 0001 from vendor 0000 (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0000, 0x0001, empty, empty, base::ASCIIToUTF16("A"), false));

  EXPECT_EQ(base::ASCIIToUTF16("Unknown product 0001 from Google Inc."),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x0001, empty, empty, empty, false));

  EXPECT_EQ(base::ASCIIToUTF16(
                "Unknown product 0001 from Google Inc. (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x0001, empty, empty, serial_number, false));

  EXPECT_EQ(base::ASCIIToUTF16("Nexus One from Google Inc."),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x4E11, empty, empty, empty, true));

  EXPECT_EQ(base::ASCIIToUTF16("Nexus One from Google Inc. (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x4E11, empty, empty, serial_number, true));

  EXPECT_EQ(base::ASCIIToUTF16("Nexus One"),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x4E11, empty, empty, empty, false));

  EXPECT_EQ(base::ASCIIToUTF16("Nexus One (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x18D1, 0x4E11, empty, empty, serial_number, false));

  EXPECT_EQ(base::ASCIIToUTF16("Unknown product 0001 from ACME"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0000, 0x0001, manufacturer, empty, empty, false));

  EXPECT_EQ(
      base::ASCIIToUTF16("Unknown product 0001 from ACME (serial number A)"),
      DevicePermissionsManager::GetPermissionMessage(
          0x0000, 0x0001, manufacturer, empty, serial_number, false));

  EXPECT_EQ(base::ASCIIToUTF16("Widget from ACME"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0001, 0x0000, manufacturer, product, empty, true));

  EXPECT_EQ(base::ASCIIToUTF16("Widget from ACME (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0001, 0x0000, manufacturer, product, serial_number, true));

  EXPECT_EQ(base::ASCIIToUTF16("Widget"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0001, 0x0000, manufacturer, product, empty, false));

  EXPECT_EQ(base::ASCIIToUTF16("Widget (serial number A)"),
            DevicePermissionsManager::GetPermissionMessage(
                0x0001, 0x0000, manufacturer, product, serial_number, false));
}

}  // namespace extensions
