// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/device_settings_lacros.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Mock observer used to observe device setting updates.
class MockDeviceSettingsObserver : public DeviceSettingsLacros::Observer {
 public:
  MockDeviceSettingsObserver() = default;
  ~MockDeviceSettingsObserver() override = default;

  MOCK_METHOD(void, OnDeviceSettingsUpdated, (), (override));
};

class DeviceSettingsLacrosTest : public ::testing::Test {
 protected:
  void SetUp() override { device_settings_lacros_.AddObserver(&observer_); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  DeviceSettingsLacros device_settings_lacros_;
  MockDeviceSettingsObserver observer_;
};

TEST_F(DeviceSettingsLacrosTest, NotifyRegisteredObserversOnSettingsUpdate) {
  crosapi::mojom::DeviceSettingsPtr device_settings =
      crosapi::mojom::DeviceSettings::New();
  device_settings->report_device_network_status =
      crosapi::mojom::DeviceSettings::OptionalBool::kTrue;

  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnDeviceSettingsUpdated()).WillOnce([&]() {
    // Verify we retrieve the updated device setting.
    const crosapi::mojom::DeviceSettings* const device_settings =
        device_settings_lacros_.GetDeviceSettings();
    EXPECT_THAT(
        device_settings->report_device_network_status,
        ::testing::Eq(crosapi::mojom::DeviceSettings::OptionalBool::kTrue));
    run_loop.Quit();
  });
  device_settings_lacros_.UpdateDeviceSettings(std::move(device_settings));
  run_loop.Run();
}

TEST_F(DeviceSettingsLacrosTest,
       ShouldNotNotifyUnregisteredObserversOnSettingsUpdate) {
  crosapi::mojom::DeviceSettingsPtr device_settings =
      crosapi::mojom::DeviceSettings::New();
  device_settings->report_device_network_status =
      crosapi::mojom::DeviceSettings::OptionalBool::kTrue;

  // Unregister observer.
  device_settings_lacros_.RemoveObserver(&observer_);

  EXPECT_CALL(observer_, OnDeviceSettingsUpdated()).Times(0);
  device_settings_lacros_.UpdateDeviceSettings(std::move(device_settings));
  task_environment_.RunUntilIdle();
}

}  // namespace
