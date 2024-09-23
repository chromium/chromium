// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/connected_input_devices_log_source.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace system_logs {

class ConnectedInputDevicesLogSourceTest : public ::testing::Test {
 public:
  ConnectedInputDevicesLogSourceTest() {}
  ConnectedInputDevicesLogSourceTest(
      const ConnectedInputDevicesLogSourceTest&) = delete;
  ConnectedInputDevicesLogSourceTest& operator=(
      const ConnectedInputDevicesLogSourceTest&) = delete;
  ~ConnectedInputDevicesLogSourceTest() override {
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    ui::DeviceDataManager::CreateInstance();
    ash::cros_healthd::FakeCrosHealthd::Initialize();
  }

  void TearDown() override { ui::DeviceDataManager::DeleteInstance(); }

  void SetCrosHealthdTouchpad(ash::cros_healthd::mojom::TelemetryInfoPtr& info,
                              std::string driver_name) {
    auto input_info = ash::cros_healthd::mojom::InputInfo::New();
    input_info->touchpad_devices =
        std::vector<ash::cros_healthd::mojom::TouchpadDevicePtr>{};

    auto touchpad_device = ash::cros_healthd::mojom::TouchpadDevice::New();
    touchpad_device->driver_name = driver_name;
    auto touchpad_input_device = ash::cros_healthd::mojom::InputDevice::New();
    touchpad_device->input_device = std::move(touchpad_input_device);

    input_info->touchpad_devices->push_back(std::move(touchpad_device));

    info->input_result = ash::cros_healthd::mojom::InputResult::NewInputInfo(
        std::move(input_info));
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  static const uint16_t unknown_vid;
  static const char unknown_vendor_name[];

  size_t num_callback_calls() const { return num_callback_calls_; }
  const SystemLogsResponse& response() const { return response_; }

  SysLogsSourceCallback fetch_callback(base::OnceClosure quit_closure) {
    return base::BindOnce(&ConnectedInputDevicesLogSourceTest::OnFetchComplete,
                          base::Unretained(this), std::move(quit_closure));
  }

  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;

  raw_ptr<base::test::ScopedCommandLine> command_line_;

 private:
  void OnFetchComplete(base::OnceClosure quit_closure,
                       std::unique_ptr<SystemLogsResponse> response) {
    ++num_callback_calls_;
    response_ = *response;
    std::move(quit_closure).Run();
  }

  // Used to verify that OnGetFeedbackData was called the correct number of
  // times.
  size_t num_callback_calls_ = 0;

  // Stores results from the log source passed into fetch_callback().
  SystemLogsResponse response_;
};

// Obsolete VID of Fry's Electronics, unlikely to be used.
const uint16_t ConnectedInputDevicesLogSourceTest::unknown_vid = 0x0001;
const char ConnectedInputDevicesLogSourceTest::unknown_vendor_name[] = "0x0001";

TEST_F(ConnectedInputDevicesLogSourceTest, Touchpad_single) {
  const std::string vendor_name = "Synaptics";
  const uint16_t vid = 0x06cb;
  const uint16_t pid = 0x685f;
  const std::string driver_name = "SynPS/2";

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({ui::TouchpadDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), vid, pid, /*version=*/0)});

  auto telem_info = ash::cros_healthd::mojom::TelemetryInfo::New();

  SetCrosHealthdTouchpad(telem_info, driver_name);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telem_info);

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(3U, response().size());

  auto it = response().find("TOUCHPAD_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(vendor_name, it->second);
  it = response().find("TOUCHPAD_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(driver_name, it->second);

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Flex_touchpad_single) {
  const std::string vendor_name = "Synaptics";
  const uint16_t vid = 0x06cb;
  const uint16_t pid = 0x685f;
  const std::string driver_name = "SynPS/2";
  const std::string flex_library_name = "libinput";

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({ui::TouchpadDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), vid, pid, /*version=*/0)});

  auto telem_info = ash::cros_healthd::mojom::TelemetryInfo::New();

  SetCrosHealthdTouchpad(telem_info, driver_name);

  telem_info->input_result->get_input_info()->touchpad_library_name =
      flex_library_name;
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telem_info);
  command_line_->GetProcessCommandLine()->AppendSwitch(
      ash::switches::kRevenBranding);

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(4U, response().size());

  auto it = response().find("TOUCHPAD_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(vendor_name, it->second);
  it = response().find("TOUCHPAD_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(driver_name, it->second);
  it = response().find("TOUCHPAD_LIBRARY");
  EXPECT_EQ(flex_library_name, it->second);

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Touchpad_single_other_ext) {
  const std::string t1_vendor_name = "Raydium";
  const uint16_t t1_vid = 0x2386;
  const uint16_t t1_pid = 0x3b1d;
  const uint16_t t2_vid = 0x0457;
  const uint16_t t2_pid = 0x3462;
  const std::string t1_driver_name = "driver1";
  const ui::TouchpadDevice tp1 = ui::TouchpadDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), t1_vid, t1_pid, /*version=*/0);
  const ui::TouchpadDevice tp2 = ui::TouchpadDevice(
      /*id=*/2, ui::INPUT_DEVICE_USB, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), t2_vid, t2_pid, /*version=*/0);

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({tp1, tp2});

  auto telem_info = ash::cros_healthd::mojom::TelemetryInfo::New();

  // Currently healthD only pulls internal touchpad data, so only that needs
  // to be set
  SetCrosHealthdTouchpad(telem_info, t1_driver_name);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telem_info);

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(3U, response().size());

  auto it = response().find("TOUCHPAD_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(t1_vendor_name, it->second);
  it = response().find("TOUCHPAD_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", t1_pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(t1_driver_name, it->second);

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Touchpad_single_ts_ext) {
  const std::string tp_vendor_name = "Elan";
  const uint16_t tp_vid = 0x04f3;
  const uint16_t tp_pid = 0x323b;
  const uint16_t ts_vid = 0x056a;
  const uint16_t ts_pid = 0xd4f4;
  const std::string driver_name = "ElanPS/2";
  const ui::TouchpadDevice tp = ui::TouchpadDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), tp_vid, tp_pid, /*version=*/0);
  const ui::InputDevice ts = ui::InputDevice(
      /*id=*/2, ui::INPUT_DEVICE_BLUETOOTH, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), ts_vid, ts_pid, /*version=*/0);

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({tp});
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({{ts, gfx::Size(),
                                                         /*touch_points=*/0}});

  auto telem_info = ash::cros_healthd::mojom::TelemetryInfo::New();

  SetCrosHealthdTouchpad(telem_info, driver_name);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telem_info);

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(3U, response().size());

  auto it = response().find("TOUCHPAD_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(tp_vendor_name, it->second);
  it = response().find("TOUCHPAD_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", tp_pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(driver_name, it->second);

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Touchscreen_single) {
  const std::string vendor_name = "Atmel";
  const uint16_t vid = 0x03eb;
  const uint16_t pid = 0x1234;

  auto input = ui::InputDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), vid, pid, /*version=*/0);
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {{input, gfx::Size(), /*touch_points=*/0}});

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find("TOUCHSCREEN_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(vendor_name, it->second);
  it = response().find("TOUCHSCREEN_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(it, response().end());

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Touchscreen_single_other_ext) {
  const std::string t1_vendor_name = "Novatek";
  const uint16_t t1_vid = 0x0603;
  const uint16_t t1_pid = 0xd124;
  const uint16_t t2_vid = 0x1fd2;
  const uint16_t t2_pid = 0x0034;

  ui::InputDevice i1 = ui::InputDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), t1_vid, t1_pid, /*version=*/0);
  ui::InputDevice i2 = ui::InputDevice(
      /*id=*/2, ui::INPUT_DEVICE_BLUETOOTH, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), t2_vid, t2_pid, /*version=*/0);
  ui::TouchscreenDevice ts1 =
      ui::TouchscreenDevice(i1, gfx::Size(), /*touch_points=*/0);
  ui::TouchscreenDevice ts2 =
      ui::TouchscreenDevice(i2, gfx::Size(), /*touch_points=*/0);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({ts1, ts2});

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find("TOUCHSCREEN_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(t1_vendor_name, it->second);
  it = response().find("TOUCHSCREEN_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", t1_pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(it, response().end());

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Touchscreen_single_tp_ext) {
  const uint16_t tp_vid = 0x04f3;
  const uint16_t tp_pid = 0x323b;
  const std::string ts_vendor_name = "Wacom";
  const uint16_t ts_vid = 0x056a;
  const uint16_t ts_pid = 0xd4f4;
  const ui::TouchpadDevice tp = ui::TouchpadDevice(
      /*id=*/1, ui::INPUT_DEVICE_USB, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), tp_vid, tp_pid, /*version=*/0);
  const ui::InputDevice ts = ui::InputDevice(
      /*id=*/2, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), ts_vid, ts_pid, /*version=*/0);

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({tp});
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {{ts, gfx::Size(), /*touch_points=*/0}});

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find("TOUCHSCREEN_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(ts_vendor_name, it->second);
  it = response().find("TOUCHSCREEN_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", ts_pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(it, response().end());

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Touchpad_single_touchscreen_single) {
  const std::string tp_vendor_name = "Zinitix";
  const uint16_t tp_vid = 0x14e5;
  const uint16_t tp_pid = 0x0901;
  std::string driver_name = "psmouse";
  const std::string ts_vendor_name = "Google";
  const uint16_t ts_vid = 0x18d1;
  const uint16_t ts_pid = 0x5400;

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({ui::TouchpadDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), tp_vid, tp_pid, /*version=*/0)});

  const auto input = ui::InputDevice(
      /*id=*/2, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), ts_vid, ts_pid, /*version=*/0);
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {{input, gfx::Size(), /*touch_points=*/0}});

  auto telem_info = ash::cros_healthd::mojom::TelemetryInfo::New();

  SetCrosHealthdTouchpad(telem_info, driver_name);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telem_info);

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(5U, response().size());

  auto it = response().find("TOUCHPAD_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(tp_vendor_name, it->second);
  it = response().find("TOUCHPAD_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", tp_pid), it->second);
  it = response().find("TOUCHSCREEN_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(ts_vendor_name, it->second);
  it = response().find("TOUCHSCREEN_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", ts_pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(driver_name, it->second);

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Touchpad_unknown_vendor_single) {
  const uint16_t pid = 0x0002;
  std::string driver_name = "psmouse";

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({ui::TouchpadDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), unknown_vid, pid, /*version=*/0)});

  auto telem_info = ash::cros_healthd::mojom::TelemetryInfo::New();

  SetCrosHealthdTouchpad(telem_info, driver_name);
  ash::cros_healthd::FakeCrosHealthd::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telem_info);

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(3U, response().size());

  auto it = response().find("TOUCHPAD_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(unknown_vendor_name, it->second);
  it = response().find("TOUCHPAD_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(driver_name, it->second);

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, Touchscreen_unknown_vendor_single) {
  const uint16_t pid = 0x0003;
  const auto input = ui::InputDevice(
      /*id=*/1, ui::INPUT_DEVICE_INTERNAL, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), unknown_vid, pid, /*version=*/0);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {{input, gfx::Size(), /*touch_points=*/0}});

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(2U, response().size());

  auto it = response().find("TOUCHSCREEN_VENDOR");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(unknown_vendor_name, it->second);
  it = response().find("TOUCHSCREEN_PID");
  ASSERT_NE(it, response().end());
  EXPECT_EQ(base::StringPrintf("0x%04x", pid), it->second);
  it = response().find("TOUCHPAD_DRIVERS");
  EXPECT_EQ(it, response().end());

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

TEST_F(ConnectedInputDevicesLogSourceTest, No_internal_touch_input) {
  const uint16_t tp_vid = 0x03eb;
  const uint16_t tp_pid = 0x1234;
  const uint16_t ts_vid = 0x04b4;
  const uint16_t ts_pid = 0x0763;

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({ui::TouchpadDevice(
      /*id=*/1, ui::INPUT_DEVICE_USB, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), tp_vid, tp_pid, /*version=*/0)});

  const auto input = ui::InputDevice(
      /*id=*/2, ui::INPUT_DEVICE_BLUETOOTH, /*name=*/"", /*phys=*/"",
      /*sys_path=*/base::FilePath(), ts_vid, ts_pid, /*version=*/0);
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {{input, gfx::Size(), /*touch_points=*/0}});

  /* Fetch log data. */
  ConnectedInputDevicesLogSource source;
  base::RunLoop run_loop;
  source.Fetch(fetch_callback(run_loop.QuitClosure()));
  run_loop.Run();

  /* Verify fetch_callback() has been called. */
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(0U, response().size());

  /* If size 0, then no point in checking ids. */

  /* Verify fetch_callback() has not been called again. */
  EXPECT_EQ(1U, num_callback_calls());
}

}  // namespace system_logs
