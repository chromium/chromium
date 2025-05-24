// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/light_provider_mojo.h"

#include <map>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_observer.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "chromeos/components/sensors/fake_sensor_hal_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

constexpr int32_t kFakeAcpiAlsId = 1;
constexpr int32_t kFakeBaseLightId = 2;
constexpr int32_t kFakeLidLightId = 3;

constexpr int64_t kFakeSampleData = 50;

constexpr char kCrosECLightName[] = "cros-ec-light";
constexpr char kAcpiAlsName[] = "acpi-als";

}  // namespace

class LightProviderMojoTest : public testing::Test {
 protected:
  void SetUp() override {
    chromeos::sensors::SensorHalDispatcher::Initialize();
    sensor_hal_server_ =
        std::make_unique<chromeos::sensors::FakeSensorHalServer>();

    als_reader_ = std::make_unique<AlsReader>();

    als_reader_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    chromeos::sensors::SensorHalDispatcher::Shutdown();
  }

  void SetProvider() {
    provider_ = std::make_unique<LightProviderMojo>(als_reader_.get());
  }

  void AddDevice(int32_t iio_device_id,
                 const std::optional<std::string> name,
                 const std::optional<std::string> location) {
    std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data(
        1);
    channels_data[0].id = chromeos::sensors::mojom::kLightChannel;
    channels_data[0].sample_data = kFakeSampleData;

    std::unique_ptr<chromeos::sensors::FakeSensorDevice> sensor_device(
        new chromeos::sensors::FakeSensorDevice(std::move(channels_data)));

    sensor_devices_[iio_device_id] = sensor_device.get();

    if (name.has_value()) {
      sensor_device->SetAttribute(chromeos::sensors::mojom::kDeviceName,
                                  name.value());
    }

    if (location.has_value()) {
      sensor_device->SetAttribute(chromeos::sensors::mojom::kLocation,
                                  location.value());
    }

    sensor_hal_server_->GetSensorService()->SetDevice(
        iio_device_id,
        std::set<chromeos::sensors::mojom::DeviceType>{
            chromeos::sensors::mojom::DeviceType::LIGHT},
        std::move(sensor_device));
  }

  void StartConnection() {
    chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
        sensor_hal_server_->PassRemote());
  }

  void TriggerNewDevicesTimeout() { provider_->OnNewDevicesTimeout(); }

  void CheckValues(int32_t iio_device_id) {
    EXPECT_TRUE(sensor_hal_server_->GetSensorService()->HasReceivers());
    EXPECT_TRUE(sensor_devices_.find(iio_device_id) != sensor_devices_.end());
    EXPECT_TRUE(sensor_devices_[iio_device_id]->HasReceivers());

    EXPECT_EQ(fake_observer_.status(), AlsReader::AlsInitStatus::kSuccess);
    EXPECT_EQ(fake_observer_.num_received_ambient_lights(), ++num_samples_);
    EXPECT_EQ(fake_observer_.ambient_light(), kFakeSampleData);
  }

  FakeObserver fake_observer_;
  std::unique_ptr<chromeos::sensors::FakeSensorHalServer> sensor_hal_server_;
  std::unique_ptr<AlsReader> als_reader_;
  std::unique_ptr<LightProviderMojo> provider_;

  std::map<int32_t,
           raw_ptr<chromeos::sensors::FakeSensorDevice, CtnExperimental>>
      sensor_devices_;

  int num_samples_ = 0;

  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(LightProviderMojoTest, AssumingAcpiAlsWithoutDeviceNameWithOneSensor) {
  SetProvider();
  AddDevice(kFakeAcpiAlsId, std::nullopt, std::nullopt);

  StartConnection();

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeAcpiAlsId);
}

TEST_F(LightProviderMojoTest, PreferCrosECLight) {
  SetProvider();
  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, std::nullopt);
  AddDevice(kFakeLidLightId, kCrosECLightName, std::nullopt);

  StartConnection();

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeLidLightId);
}

TEST_F(LightProviderMojoTest, GetSamplesFromLidLights) {
  SetProvider();
  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, std::nullopt);
  AddDevice(kFakeBaseLightId, kCrosECLightName,
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(kFakeLidLightId, kCrosECLightName,
            chromeos::sensors::mojom::kLocationLid);

  StartConnection();

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeLidLightId);

  // Simulate a disconnection of the accelerometer's mojo channel in IIO
  // Service.
  AddDevice(kFakeLidLightId, kCrosECLightName,
            chromeos::sensors::mojom::kLocationLid);

  // Wait until the disconnection is done.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sensor_hal_server_->GetSensorService()->HasReceivers());

  // Simulate a disconnection of IIO Service.
  sensor_hal_server_->GetSensorService()->ClearReceivers();
  sensor_hal_server_->OnServerDisconnect();

  // Wait until the disconnect arrives at SensorHalDispatcher.
  base::RunLoop().RunUntilIdle();

  StartConnection();

  // Wait until samples are received.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeLidLightId);
}

TEST_F(LightProviderMojoTest, PreferLateCrosECLight) {
  SetProvider();
  StartConnection();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_observer_.has_status());

  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, std::nullopt);

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  // Acpi-als is used.
  CheckValues(kFakeAcpiAlsId);

  AddDevice(kFakeLidLightId, kCrosECLightName, std::nullopt);

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  // Simulate the timeout.
  TriggerNewDevicesTimeout();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  // Cros-ec-light overwrites the acpi-als.
  CheckValues(kFakeLidLightId);
}

TEST_F(LightProviderMojoTest, GetSamplesFromLateLidLights) {
  SetProvider();
  StartConnection();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_observer_.has_status());

  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, std::nullopt);
  AddDevice(kFakeBaseLightId, kCrosECLightName,
            chromeos::sensors::mojom::kLocationBase);

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeBaseLightId);

  AddDevice(kFakeLidLightId, kCrosECLightName,
            chromeos::sensors::mojom::kLocationLid);

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  // Simulate the timeout.
  TriggerNewDevicesTimeout();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeLidLightId);
}

TEST_F(LightProviderMojoTest, DeviceRemoved) {
  SetProvider();
  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, std::nullopt);
  AddDevice(kFakeBaseLightId, kCrosECLightName,
            chromeos::sensors::mojom::kLocationBase);
  AddDevice(kFakeLidLightId, kCrosECLightName,
            chromeos::sensors::mojom::kLocationLid);

  StartConnection();

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  // cros-ec-light on lid is the highest priority.
  CheckValues(kFakeLidLightId);

  sensor_devices_[kFakeAcpiAlsId]->ClearReceiversWithReason(
      chromeos::sensors::mojom::SensorDeviceDisconnectReason::DEVICE_REMOVED,
      "Device was removed");

  // Wait until the disconnection is done.
  base::RunLoop().RunUntilIdle();

  // The sensor service is not reset with the reason: DEVICE_REMOVED.
  EXPECT_TRUE(sensor_hal_server_->GetSensorService()->HasReceivers());

  // Wait until samples are received.
  base::RunLoop().RunUntilIdle();

  sensor_devices_[kFakeLidLightId]->ClearReceiversWithReason(
      chromeos::sensors::mojom::SensorDeviceDisconnectReason::DEVICE_REMOVED,
      "Device was removed");
  // Overwrite the lid light sensor in the iioservice.
  AddDevice(kFakeLidLightId, "", std::nullopt);

  // Wait until the disconnection and LightProviderMojo::ResetStates are done.
  base::RunLoop().RunUntilIdle();

  // Simulate the timeout.
  TriggerNewDevicesTimeout();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeBaseLightId);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
