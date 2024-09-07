// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/controlled_frame/controlled_frame_permission_request_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/usb/usb_browser_test_utils.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace {
constexpr char kFakeUsbDeviceSerialNumber[] = "123456";
constexpr char kUsbError[] = "error";
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
                                                kUsbError);
  test_case.policy_features.insert(
      blink::mojom::PermissionsPolicyFeature::kUsb);
  test_case.success_result = kFakeUsbDeviceSerialNumber;
  test_case.failure_result = kUsbError;

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

}  // namespace controlled_frame
