// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/input_data_provider_touch.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace ash::diagnostics {

namespace {

constexpr uint32_t kEvdevId = 7;
constexpr char kFilePath[] = "/dev/input/event7";
constexpr mojom::ConnectionType kConnectionType =
    mojom::ConnectionType::kInternal;

ui::InputDevice InputDeviceFromCapabilities(
    int device_id,
    const ui::DeviceCapabilities& capabilities) {
  ui::EventDeviceInfo device_info = {};
  ui::CapabilitiesToDeviceInfo(capabilities, &device_info);

  const std::string sys_path =
      base::StringPrintf("/dev/input/event%d-%s", device_id, capabilities.path);

  return ui::InputDevice(device_id, device_info.device_type(),
                         device_info.name(), device_info.phys(),
                         base::FilePath(sys_path), device_info.vendor_id(),
                         device_info.product_id(), device_info.version());
}

}  // namespace

class InputDataProviderTouchTest : public ash::AshTestBase {
 public:
  InputDataProviderTouchTest()
      : AshTestBase(content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  InputDataProviderTouchTest(const InputDataProviderTouchTest&) = delete;
  InputDataProviderTouchTest& operator=(const InputDataProviderTouchTest&) =
      delete;
  ~InputDataProviderTouchTest() override = default;

  void SetUp() override {
    input_data_provider_touch_ = std::make_unique<InputDataProviderTouch>();

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    touch_device_info_.reset();
  }

  // Initialize input device by device capabilities, e.g. kEveTouchScreen.
  void InitInputDeviceInformation(ui::DeviceCapabilities capabilities) {
    device_information.evdev_id = kEvdevId;
    device_information.path = base::FilePath(kFilePath);
    device_information.connection_type = kConnectionType;

    ui::CapabilitiesToDeviceInfo(capabilities,
                                 &device_information.event_device_info);

    device_information.input_device =
        InputDeviceFromCapabilities(kEvdevId, capabilities);
  }

 protected:
  InputDeviceInformation device_information;
  std::unique_ptr<InputDataProviderTouch> input_data_provider_touch_;
  mojom::TouchDeviceInfoPtr touch_device_info_;
};

TEST_F(InputDataProviderTouchTest, ConstructTouchscreenDeviceInfo) {
  InitInputDeviceInformation(ui::kEveTouchScreen);

  touch_device_info_ = input_data_provider_touch_->ConstructTouchDevice(
      &device_information, /*is_internal_display_on=*/true);

  EXPECT_EQ(kEvdevId, touch_device_info_->id);
  EXPECT_EQ(kConnectionType, touch_device_info_->connection_type);
  EXPECT_EQ(mojom::TouchDeviceType::kDirect, touch_device_info_->type);
  EXPECT_EQ(device_information.event_device_info.name(),
            touch_device_info_->name);
  EXPECT_TRUE(touch_device_info_->testable);
}

TEST_F(InputDataProviderTouchTest,
       ConstructTouchscreenDeviceInfoWithDisplayOff) {
  InitInputDeviceInformation(ui::kEveTouchScreen);

  touch_device_info_ = input_data_provider_touch_->ConstructTouchDevice(
      &device_information, /*is_internal_display_on=*/false);

  // The internal touchscreen is expected to be untestable.
  EXPECT_FALSE(touch_device_info_->testable);
}

}  // namespace ash::diagnostics
