// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/bluetooth/web_bluetooth_test_utils.h"
#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/usb/usb_browser_test_utils.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/bluetooth_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace {

using testing::Return;

constexpr char kFailResult[] = "error";

// USB
constexpr char kFakeUsbDeviceSerialNumber[] = "123456";

// Serial
constexpr char kExpectedPortsLength[] = "1";

// Bluetooth
constexpr char kFakeBluetoothDeviceName[] = "Test Device";
constexpr char kDeviceAddress[] = "00:00:00:00:00:00";
constexpr char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";

}  // namespace

namespace controlled_frame {

class ControlledFrameDisabledPermissionTest
    : public ControlledFramePermissionRequestTestBase,
      public testing::WithParamInterface<DisabledPermissionTestParam> {};

class ControlledFrameDisabledPermissionUsbTest
    : public ControlledFrameDisabledPermissionTest {
 public:
  ControlledFrameDisabledPermissionUsbTest() = default;
  ~ControlledFrameDisabledPermissionUsbTest() override = default;

  void SetUpOnMainThread() override {
    ControlledFrameDisabledPermissionTest::SetUpOnMainThread();
    fake_device_info_ = device_manager_.CreateAndAddDevice(
        0, 0, "Test Manufacturer", "Test Device", kFakeUsbDeviceSerialNumber);
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    UsbChooserContextFactory::GetForProfile(browser()->profile())
        ->SetDeviceManagerForTesting(std::move(device_manager));

    test_content_browser_client_.SetAsBrowserClient();
  }

  void TearDownOnMainThread() override {
    test_content_browser_client_.UnsetAsBrowserClient();
    ControlledFrameDisabledPermissionTest::TearDownOnMainThread();
  }

  void UseFakeChooser() {
    test_content_browser_client_.delegate().UseFakeChooser();
  }

 private:
  device::FakeUsbDeviceManager device_manager_;
  device::mojom::UsbDeviceInfoPtr fake_device_info_;
  TestUsbContentBrowserClient test_content_browser_client_;
};

IN_PROC_BROWSER_TEST_P(ControlledFrameDisabledPermissionUsbTest, WebUSB) {
  UseFakeChooser();

  DisabledPermissionTestCase test_case;
  test_case.request_script = content::JsReplace(R"(
(async () => {
  try {
    const device =
      await navigator.usb.requestDevice({ filters: [] });
    return device.serialNumber;
  } catch (_) {
    return $1;
  }
})();
  )",
                                                kFailResult);
  test_case.policy_features.insert(
      blink::mojom::PermissionsPolicyFeature::kUsb);
  test_case.success_result = kFakeUsbDeviceSerialNumber;
  test_case.failure_result = kFailResult;

  DisabledPermissionTestParam test_param = GetParam();
  VerifyDisabledPermission(test_case, test_param);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFrameDisabledPermissionUsbTest,
                         testing::ValuesIn(
                             GetDefaultDisabledPermissionTestParams()),
                         [](const testing::TestParamInfo<
                             DisabledPermissionTestParam>& info) {
                           return info.param.name;
                         });

class ControlledFrameDisabledPermissionSerialTest
    : public ControlledFrameDisabledPermissionTest {
 public:
  void SetUpOnMainThread() override {
    ControlledFrameDisabledPermissionTest::SetUpOnMainThread();
    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());
    context()->SetPortManagerForTesting(std::move(port_manager));
  }

  void TearDownOnMainThread() override {
    ControlledFrameDisabledPermissionTest::TearDownOnMainThread();
  }

  device::FakeSerialPortManager& port_manager() { return port_manager_; }
  SerialChooserContext* context() {
    return SerialChooserContextFactory::GetForProfile(browser()->profile());
  }

  void CreatePortAndGrantPermissionToOrigin(const url::Origin& origin) {
    // Create port and grant permission to it.
    auto port = device::mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    context()->GrantPortPermission(origin, *port);
    port_manager().AddPort(std::move(port));
  }

 private:
  device::FakeSerialPortManager port_manager_;
};

IN_PROC_BROWSER_TEST_P(ControlledFrameDisabledPermissionSerialTest, WebSerial) {
  DisabledPermissionTestCase test_case;
  test_case.request_script = content::JsReplace(R"(
(async () => {
  try {
    const ports =
      await navigator.serial.getPorts().then(ports => ports.length);
    return ports !== 0 ? ports.toString() : $1;
  } catch (_) {
    return $1;
  }
})()
  )",
                                                kFailResult);

  test_case.policy_features.insert(
      blink::mojom::PermissionsPolicyFeature::kSerial);
  test_case.success_result = kExpectedPortsLength;
  test_case.failure_result = kFailResult;

  DisabledPermissionTestParam test_param = GetParam();
  auto [app_frame, controlled_frame] =
      SetUpControlledFrame(test_case, test_param);
  if (!app_frame || !controlled_frame) {
    return;
  }
  CreatePortAndGrantPermissionToOrigin(app_frame->GetLastCommittedOrigin());
  CreatePortAndGrantPermissionToOrigin(
      controlled_frame->GetLastCommittedOrigin());
  VerifyDisabledPermission(test_case, test_param, app_frame, controlled_frame);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFrameDisabledPermissionSerialTest,
                         testing::ValuesIn(
                             GetDefaultDisabledPermissionTestParams()),
                         [](const testing::TestParamInfo<
                             DisabledPermissionTestParam>& info) {
                           return info.param.name;
                         });

class ControlledFrameDisabledPermissionWebBluetoothTest
    : public ControlledFrameDisabledPermissionTest {
 public:
  void SetUpOnMainThread() override {
    ControlledFrameDisabledPermissionTest::SetUpOnMainThread();
    // Hook up the test bluetooth delegate.
    SetFakeBlueboothAdapter();
    old_browser_client_ = content::SetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    content::SetBrowserClientForTesting(old_browser_client_);
    ControlledFrameDisabledPermissionTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for accessing navigator.bluetooth.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetFakeBlueboothAdapter() {
    adapter_ = new FakeBluetoothAdapter();
    EXPECT_CALL(*adapter_, IsPresent()).WillRepeatedly(Return(true));
    EXPECT_CALL(*adapter_, IsPowered()).WillRepeatedly(Return(true));
    content::SetBluetoothAdapter(adapter_);
  }

  void AddFakeDevice(const std::string& device_address) {
    const device::BluetoothUUID kHeartRateUUID(kHeartRateUUIDString);
    auto fake_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), /*bluetooth_class=*/0u, kFakeBluetoothDeviceName,
            device_address, /*paired=*/true, /*connected=*/true);
    fake_device->AddUUID(kHeartRateUUID);
    fake_device->AddMockService(
        std::make_unique<testing::NiceMock<device::MockBluetoothGattService>>(
            fake_device.get(), kHeartRateUUIDString, kHeartRateUUID,
            /*is_primary=*/true));
    adapter_->AddMockDevice(std::move(fake_device));
  }

  void SetDeviceToSelect(const std::string& device_address) {
    browser_client_.bluetooth_delegate()->SetDeviceToSelect(device_address);
  }

 private:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  BluetoothTestContentBrowserClient browser_client_;
  raw_ptr<content::ContentBrowserClient> old_browser_client_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(ControlledFrameDisabledPermissionWebBluetoothTest,
                       WebBluetooth) {
  DisabledPermissionTestCase test_case;
  test_case.request_script = content::JsReplace(R"(
(async () => {
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{services: ['heart_rate']}]
    });
    return device.name;
  } catch (e) {
    return $1;
  }
})();
  )",
                                                kFailResult);
  test_case.policy_features.insert(
      blink::mojom::PermissionsPolicyFeature::kBluetooth);
  test_case.success_result = kFakeBluetoothDeviceName;
  test_case.failure_result = kFailResult;

  DisabledPermissionTestParam test_param = GetParam();
  auto [app_frame, controlled_frame] =
      SetUpControlledFrame(test_case, test_param);
  if (!app_frame || !controlled_frame) {
    return;
  }

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  VerifyDisabledPermission(test_case, test_param, app_frame, controlled_frame);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/
                         ,
                         ControlledFrameDisabledPermissionWebBluetoothTest,
                         testing::ValuesIn(
                             GetDefaultDisabledPermissionTestParams()),
                         [](const testing::TestParamInfo<
                             DisabledPermissionTestParam>& info) {
                           return info.param.name;
                         });

}  // namespace controlled_frame
