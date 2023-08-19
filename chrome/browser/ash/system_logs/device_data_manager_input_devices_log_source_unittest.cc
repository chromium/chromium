// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <vector>

#include "chrome/browser/ash/system_logs/device_data_manager_input_devices_log_source.h"

#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/gfx/geometry/size.h"

using content::BrowserThread;
using ::testing::Contains;
using ::testing::EndsWith;
using ::testing::Eq;
using ::testing::HasSubstr;

namespace system_logs {

namespace {

constexpr char kDeviceDataManagerLogKey[] = "ui_device_data_manager_devices";

constexpr char kDeviceDataManagerCountsLogKey[] =
    "ui_device_data_manager_device_counts";

// Four input sources, five categories of devices.
constexpr int kTotalCounts = 4 * 5;

// Splits multi-line block of text into array of strings.
std::vector<std::string> SplitLines(const std::string& param) {
  return base::SplitString(param, "\n", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

class DeviceDataManagerInputDevicesLogSourceTest : public ::testing::Test {
 public:
  DeviceDataManagerInputDevicesLogSourceTest() {}
  DeviceDataManagerInputDevicesLogSourceTest(
      const DeviceDataManagerInputDevicesLogSourceTest&) = delete;
  DeviceDataManagerInputDevicesLogSourceTest& operator=(
      const DeviceDataManagerInputDevicesLogSourceTest&) = delete;
  ~DeviceDataManagerInputDevicesLogSourceTest() override = default;

  // ::testing::Test:
  void SetUp() override { ui::DeviceDataManager::CreateInstance(); }

  void TearDown() override { ui::DeviceDataManager::DeleteInstance(); }

 protected:
  size_t num_callback_calls() const { return num_callback_calls_; }
  const SystemLogsResponse& response() const { return response_; }

  // Synchronously invokes Fetch on a log source.
  void RunSourceFetch(DeviceDataManagerInputDevicesLogSource* source) {
    source->Fetch((base::BindOnce(
        &DeviceDataManagerInputDevicesLogSourceTest::OnFetchComplete,
        base::Unretained(this))));
    // Start the loop, the source fetch will exit.
    fetch_run_loop_.Run();
  }

  ui::InputDevice CreateInputDevice(
      int id,
      const ui::DeviceCapabilities& capabilities) {
    ui::EventDeviceInfo devinfo;
    EXPECT_TRUE(ui::CapabilitiesToDeviceInfo(capabilities, &devinfo));
    return ui::InputDevice(id, devinfo.device_type(), devinfo.name(),
                           devinfo.phys(), base::FilePath(capabilities.path),
                           devinfo.vendor_id(), devinfo.product_id(),
                           devinfo.version());
  }

  ui::TouchpadDevice CreateTouchpadDevice(
      int id,
      const ui::DeviceCapabilities& capabilities) {
    ui::EventDeviceInfo devinfo;
    EXPECT_TRUE(ui::CapabilitiesToDeviceInfo(capabilities, &devinfo));
    EXPECT_TRUE(devinfo.HasTouchpad());
    return ui::TouchpadDevice(id, devinfo.device_type(), devinfo.name(),
                              devinfo.phys(), base::FilePath(capabilities.path),
                              devinfo.vendor_id(), devinfo.product_id(),
                              devinfo.version(), devinfo.HasHapticTouchpad());
  }

  ui::KeyboardDevice CreateKeyboardDevice(
      int id,
      const ui::DeviceCapabilities& capabilities) {
    ui::EventDeviceInfo devinfo;
    EXPECT_TRUE(ui::CapabilitiesToDeviceInfo(capabilities, &devinfo));
    EXPECT_TRUE(devinfo.HasKeyboard());
    return ui::KeyboardDevice(id, devinfo.device_type(), devinfo.name(),
                              devinfo.phys(), base::FilePath(capabilities.path),
                              devinfo.vendor_id(), devinfo.product_id(),
                              devinfo.version(),
                              devinfo.HasKeyEvent(KEY_ASSISTANT));
  }

  ui::TouchscreenDevice CreateTouchscreenDevice(
      int id,
      bool has_stylus_garage_switch,
      const ui::DeviceCapabilities& capabilities) {
    ui::EventDeviceInfo devinfo;
    EXPECT_TRUE(ui::CapabilitiesToDeviceInfo(capabilities, &devinfo));
    EXPECT_TRUE(devinfo.HasTouchscreen());
    return ui::TouchscreenDevice(
        id, devinfo.device_type(), devinfo.name(),
        gfx::Size(devinfo.GetAbsMaximum(ABS_X) - devinfo.GetAbsMinimum(ABS_X),
                  devinfo.GetAbsMaximum(ABS_Y) - devinfo.GetAbsMinimum(ABS_Y)),
        devinfo.GetAbsMaximum(ABS_MT_SLOT) + 1, devinfo.HasStylus(),
        has_stylus_garage_switch);
  }

  // Note: CreateGamepadInput() is not needed, as DeviceDataManager does not
  // process gamepad devices directly.

 private:
  void OnFetchComplete(std::unique_ptr<SystemLogsResponse> response) {
    ++num_callback_calls_;
    response_ = *response;
    fetch_run_loop_.Quit();
  }

  // Creates the necessary browser threads.
  content::BrowserTaskEnvironment task_environment_;
  // Used to verify that OnGetFeedbackData was called the correct number of
  // times.
  size_t num_callback_calls_ = 0;

  // Stores results from the log source passed into fetch_callback().
  SystemLogsResponse response_;
  // Used for waiting on fetch results.
  base::RunLoop fetch_run_loop_;
};

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, InternalKeyboard_single) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {CreateKeyboardDevice(1, ui::kWoomaxKeyboard)});
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  // Fetch log data.
  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());
  it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());

  EXPECT_THAT(SplitLines(it->second),
              Contains(EndsWith("=0")).Times(kTotalCounts - 1));
  EXPECT_THAT(it->second, HasSubstr("count_internal_keyboard_devices=1\n"));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, InternalTouchpad_single) {
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {CreateTouchpadDevice(1, ui::kDellLatitudeE6510Touchpad)});

  // Fetch log data.
  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());
  it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());

  EXPECT_THAT(SplitLines(it->second),
              Contains(EndsWith("=0")).Times(kTotalCounts - 1));
  EXPECT_THAT(it->second, HasSubstr("count_internal_touchpad_devices=1\n"));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, ExternalKeyboard_single) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {CreateKeyboardDevice(1, ui::kLogitechKeyboardK120)});

  // Fetch log data.
  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());
  it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());

  EXPECT_THAT(SplitLines(it->second),
              Contains(EndsWith("=0")).Times(kTotalCounts - 1));
  EXPECT_THAT(it->second, HasSubstr("count_usb_keyboard_devices=1\n"));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, InternalTouchscreen_single) {
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({CreateTouchscreenDevice(
      1, /*has_stylus_garage_switch=*/false, ui::kKohakuTouchscreen)});

  // Fetch log data.
  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());
  it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());

  EXPECT_THAT(SplitLines(it->second),
              Contains(EndsWith("=0")).Times(kTotalCounts - 1));
  EXPECT_THAT(it->second, HasSubstr("count_internal_touchscreen_devices=1\n"));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest,
       InternalAndExternalTouchscreen) {
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({
      CreateTouchscreenDevice(1, /*has_stylus_garage_switch=*/false,
                              ui::kNocturneTouchScreen),
      CreateTouchscreenDevice(2, /*has_stylus_garage_switch=*/false,
                              ui::kElo_TouchSystems_2700),
  });

  // Fetch log data.
  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());
  it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());

  EXPECT_THAT(SplitLines(it->second),
              Contains(EndsWith("=0")).Times(kTotalCounts - 2));
  EXPECT_THAT(it->second, HasSubstr("count_internal_touchscreen_devices=1\n"));
  EXPECT_THAT(it->second, HasSubstr("count_usb_touchscreen_devices=1\n"));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, UnknownDevice) {
  ui::DeviceDataManagerTestApi().SetUncategorizedDevices({
      CreateInputDevice(3, ui::kSideVolumeButton),
      CreateInputDevice(4, ui::kEveStylus),
  });

  // Fetch log data.
  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());
  it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());

  EXPECT_THAT(SplitLines(it->second),
              Contains(EndsWith("=0")).Times(kTotalCounts - 2));
  // Side volume button is in EC, and on 'unknown' bus.
  EXPECT_THAT(it->second, HasSubstr("count_unknown_uncategorized_devices=1\n"));
  // Stylus is on i2c 'internal' bus.
  EXPECT_THAT(it->second,
              HasSubstr("count_internal_uncategorized_devices=1\n"));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, MultipleGamepadDevices) {
  ui::DeviceDataManagerTestApi().SetUncategorizedDevices({
      CreateInputDevice(3, ui::kXboxGamepad),      // USB
      CreateInputDevice(4, ui::kHJCGamepad),       // USB
      CreateInputDevice(5, ui::kiBuffaloGamepad),  // USB
      CreateInputDevice(8, ui::kXboxElite),        // BT
      CreateInputDevice(9, ui::kXboxElite),        // BT
  });

  // Fetch log data.
  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());
  it = response().find(kDeviceDataManagerCountsLogKey);
  ASSERT_NE(it, response().end());

  EXPECT_THAT(SplitLines(it->second),
              Contains(EndsWith("=0")).Times(kTotalCounts - 2));
  EXPECT_THAT(it->second, HasSubstr("count_usb_uncategorized_devices=3\n"));
  EXPECT_THAT(it->second,
              HasSubstr("count_bluetooth_uncategorized_devices=2\n"));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, Incomplete) {
  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  auto it = response().find(kDeviceDataManagerLogKey);
  ASSERT_NE(it, response().end());
  EXPECT_THAT(SplitLines(it->second), Contains(Eq("AreDeviceListsComplete=0")));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, Complete) {
  DeviceDataManagerInputDevicesLogSource source;
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  RunSourceFetch(&source);

  auto it = response().find(kDeviceDataManagerLogKey);
  ASSERT_NE(it, response().end());
  EXPECT_THAT(SplitLines(it->second), Contains(Eq("AreDeviceListsComplete=1")));
}

TEST_F(DeviceDataManagerInputDevicesLogSourceTest, NoDeviceDataManager) {
  ui::DeviceDataManager::DeleteInstance();

  DeviceDataManagerInputDevicesLogSource source;
  RunSourceFetch(&source);

  auto it = response().find(kDeviceDataManagerLogKey);
  ASSERT_NE(it, response().end());
  EXPECT_EQ("No DeviceDataManager instance", it->second);
}

// TODO(b/265986652): add additional unit tests for display calibration once the
// full display map logging is implemented.

}  // namespace system_logs
