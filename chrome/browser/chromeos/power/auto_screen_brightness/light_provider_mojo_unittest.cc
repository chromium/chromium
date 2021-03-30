// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/light_provider_mojo.h"

#include <map>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/fake_observer.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "chromeos/components/sensors/fake_sensor_hal_server.h"
#include "chromeos/components/sensors/sensor_hal_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

constexpr int32_t kFakeAcpiAlsId = 1;
constexpr int32_t kFakeBaseLightId = 2;
constexpr int32_t kFakeLidLightId = 3;

constexpr int64_t kFakeSampleData = 50;
const char kWrongLocation[5] = "lidd";

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

  void SetProvider(bool has_several_light_sensors) {
    provider_ = std::make_unique<LightProviderMojo>(als_reader_.get(),
                                                    has_several_light_sensors);
  }

  void AddDevice(int32_t iio_device_id,
                 const base::Optional<std::string> name,
                 const base::Optional<std::string> location) {
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

  std::map<int32_t, chromeos::sensors::FakeSensorDevice*> sensor_devices_;

  int num_samples_ = 0;

  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(LightProviderMojoTest, GetSamplesWithOneSensor) {
  SetProvider(/*has_several_light_sensors=*/false);
  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, base::nullopt);

  StartConnection();

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeAcpiAlsId);
}

TEST_F(LightProviderMojoTest, AssumingAcpiAlsWithoutDeviceNameWithOneSensor) {
  SetProvider(/*has_several_light_sensors=*/false);
  AddDevice(kFakeAcpiAlsId, base::nullopt, base::nullopt);

  StartConnection();

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeAcpiAlsId);
}

TEST_F(LightProviderMojoTest, PreferCrosECLightWithOneSensor) {
  SetProvider(/*has_several_light_sensors=*/false);
  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, base::nullopt);
  AddDevice(kFakeLidLightId, kCrosECLightName, base::nullopt);

  StartConnection();

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  CheckValues(kFakeLidLightId);
}

TEST_F(LightProviderMojoTest, InvalidLocationWithSeveralLightSensors) {
  SetProvider(/*has_several_light_sensors=*/true);
  AddDevice(kFakeLidLightId, kCrosECLightName, kWrongLocation);

  StartConnection();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  // Simulate the timeout so that the initialization fails.
  TriggerNewDevicesTimeout();

  // Wait until the mojo connection of SensorService is reset.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sensor_hal_server_->GetSensorService()->HasReceivers());
  EXPECT_EQ(fake_observer_.status(),
            AlsReader::AlsInitStatus::kIncorrectConfig);
}

TEST_F(LightProviderMojoTest, GetSamplesFromLidLightsSeveralLightSensors) {
  SetProvider(/*has_several_light_sensors=*/true);
  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, base::nullopt);
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

TEST_F(LightProviderMojoTest, PreferLateCrosECLightWithOneSensor) {
  provider_ = std::make_unique<LightProviderMojo>(als_reader_.get(),
                                                  /*has_two_sensors=*/false);
  StartConnection();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_observer_.has_status());

  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, base::nullopt);

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  // Acpi-als is used.
  CheckValues(kFakeAcpiAlsId);

  AddDevice(kFakeLidLightId, kCrosECLightName, base::nullopt);

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  // Simulate the timeout.
  TriggerNewDevicesTimeout();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  // Cros-ec-light overwrites the acpi-als.
  CheckValues(kFakeLidLightId);
}

TEST_F(LightProviderMojoTest, GetSamplesFromLateLidLightsWithTwoSensors) {
  provider_ = std::make_unique<LightProviderMojo>(als_reader_.get(),
                                                  /*has_two_sensors=*/true);
  StartConnection();

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_observer_.has_status());

  AddDevice(kFakeAcpiAlsId, kAcpiAlsName, base::nullopt);
  AddDevice(kFakeBaseLightId, kCrosECLightName,
            chromeos::sensors::mojom::kLocationBase);

  // Wait until all tasks are done.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(fake_observer_.has_status());

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

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
