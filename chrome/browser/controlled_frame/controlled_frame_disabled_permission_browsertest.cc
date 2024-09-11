// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/usb/usb_browser_test_utils.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace {
constexpr char kFakeUsbDeviceSerialNumber[] = "123456";
constexpr char kFailResult[] = "error";
constexpr char kExpectedPortsLength[] = "1";
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

}  // namespace controlled_frame
