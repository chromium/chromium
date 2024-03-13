// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_callback_support.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/usb/usb_api.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/cpp/test/mock_usb_mojo_device.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace extensions {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

constexpr char kManifest[] =
    R"(
      {
        "name": "ChromeUsbApiTest App",
        "version": "1.0",
        "manifest_version": 2,
        "app": {
          "background": {
            "scripts": ["background_script.js"]
          }
        },
        "permissions": ["usb"]
      }
    )";

// Need to fill it with an extension's url.
constexpr char kPolicySetting[] = R"(
    [
      {
        "devices": [{ "vendor_id": 0, "product_id": 0 }],
        "urls": ["%s"]
      }
    ])";

class ChromeUsbApiTest : public ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Set fake USB device manager for extensions::UsbDeviceManager.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> usb_manager;
    fake_usb_manager_.AddReceiver(usb_manager.InitWithNewPipeAndPassReceiver());
    UsbDeviceManager::Get(profile())->SetDeviceManagerForTesting(
        std::move(usb_manager));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void AddFakeDevice() {
    std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
    configs.push_back(
        device::FakeUsbDeviceInfo::CreateConfiguration(0xff, 0x00, 0x00, 1));
    configs.push_back(
        device::FakeUsbDeviceInfo::CreateConfiguration(0xff, 0x00, 0x00, 2));

    fake_device_ = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0, 0, "Test Manufacturer", "Test Device", "ABC123", std::move(configs));
    fake_usb_manager_.AddDevice(fake_device_);
    fake_usb_manager_.SetMockForDevice(fake_device_->guid(), &mock_device_);
    base::RunLoop().RunUntilIdle();
  }

  void SetUpPolicy(const Extension* extension) {
    profile()->GetPrefs()->Set(
        prefs::kManagedWebUsbAllowDevicesForUrls,
        base::test::ParseJson(base::StringPrintf(
            kPolicySetting, extension->url().spec().c_str())));
  }

  // `mock_device_`, `fake_device_`, and `fake_usb_manager_` must be declared in
  // this order to avoid dangling pointers.
  device::MockUsbMojoDevice mock_device_;
  scoped_refptr<device::FakeUsbDeviceInfo> fake_device_;
  device::FakeUsbDeviceManager fake_usb_manager_;
};

IN_PROC_BROWSER_TEST_F(ChromeUsbApiTest, GetDevicesByPolicy) {
  extensions::TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), R"(
    chrome.test.sendMessage("ready", async () => {
      chrome.usb.getDevices({
        vendorId: 0,
        productId: 0
      }, function(devices) {
        chrome.test.assertEq(1, devices.length);
        const device = devices[0];
        chrome.test.assertEq(0, device.vendorId);
        chrome.test.assertEq(0, device.productId);
        chrome.test.assertEq(0x0100, device.version);
        chrome.test.assertEq("Test Device", device.productName);
        chrome.test.assertEq("Test Manufacturer", device.manufacturerName);
        chrome.test.assertEq("ABC123", device.serialNumber);
        chrome.test.notifyPass();
      });
    });
  )");

  // Launch the test app.
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  extensions::ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(dir.UnpackedPath());

  // Run the test.
  SetUpPolicy(extension);
  AddFakeDevice();
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(ChromeUsbApiTest, GetDevicesByPolicyNoPolicySet) {
  extensions::TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), R"(
    chrome.test.sendMessage("ready", async () => {
      chrome.usb.getDevices({
        vendorId: 0,
        productId: 0
      }, function(devices) {
        chrome.test.assertEq(0, devices.length);
        chrome.test.notifyPass();
      });
    });
  )");

  // Launch the test app.
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  extensions::ResultCatcher result_catcher;
  LoadExtension(dir.UnpackedPath());

  // Run the test.
  AddFakeDevice();
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(ChromeUsbApiTest, FindDevicesByPolicy) {
  extensions::TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), R"(
    chrome.test.sendMessage("ready", async () => {
      chrome.usb.findDevices({
        vendorId: 0,
        productId: 0
      }, function(devices) {
        chrome.test.assertEq(1, devices.length);
        const device = devices[0];
        chrome.test.assertEq(0, device.vendorId);
        chrome.test.assertEq(0, device.productId);
        chrome.test.notifyPass();
      });
    });
  )");

  // Launch the test app.
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  extensions::ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(dir.UnpackedPath());

  EXPECT_CALL(mock_device_, Open)
      .WillOnce(
          RunOnceCallback<0>(device::mojom::UsbOpenDeviceResult::NewSuccess(
              device::mojom::UsbOpenDeviceSuccess::OK)));

  // Run the test.
  SetUpPolicy(extension);
  AddFakeDevice();
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(ChromeUsbApiTest, FindDevicesByPolicyNoPolicySet) {
  extensions::TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), R"(
    chrome.test.sendMessage("ready", async () => {
      chrome.usb.findDevices({
        vendorId: 0,
        productId: 0
      }, function(devices) {
        chrome.test.assertEq(undefined, devices);
        chrome.test.notifyPass();
      });
    });
  )");

  // Launch the test app.
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  extensions::ResultCatcher result_catcher;
  LoadExtension(dir.UnpackedPath());

  // Run the test.
  AddFakeDevice();
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(ChromeUsbApiTest, OnDevicesAdded) {
  extensions::TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), R"(
    chrome.usb.onDeviceAdded.addListener(function(device) {
      chrome.test.assertEq(0, device.vendorId);
      chrome.test.assertEq(0, device.productId);
      chrome.test.assertEq(0x0100, device.version);
      chrome.test.assertEq("Test Device", device.productName);
      chrome.test.assertEq("Test Manufacturer", device.manufacturerName);
      chrome.test.assertEq("ABC123", device.serialNumber);
      chrome.test.notifyPass();
    });
  )");

  // Launch the test app.
  extensions::ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(dir.UnpackedPath());

  // Run the test.
  SetUpPolicy(extension);
  AddFakeDevice();
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace
}  // namespace extensions
