// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/accelerometer/accelerometer_provider_mojo.h"

#include <memory>
#include <utility>

#include "ash/accelerometer/accelerometer_constants.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "chromeos/components/sensors/fake_sensor_hal_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr double kFakeScaleValue = 10.0;

constexpr int kFakeLidAccelerometerId = 1;
constexpr int kFakeBaseAccelerometerId = 2;
constexpr int kFakeLidAngleId = 3;

constexpr int64_t kFakeSampleData[] = {1, 2, 3};

class FakeObserver : public AccelerometerReader::Observer {
 public:
  void OnECLidAngleDriverStatusChanged(bool is_supported) override {
    is_supported_ = is_supported;
  }
  void OnAccelerometerUpdated(const AccelerometerUpdate& update) override {
    for (uint32_t index = 0; index < ACCELEROMETER_SOURCE_COUNT; ++index) {
      auto source = static_cast<AccelerometerSource>(index);
      if (!update.has(source))
        continue;

      EXPECT_EQ(update.get(source).x, kFakeSampleData[0] * kFakeScaleValue);
      EXPECT_EQ(update.get(source).y, kFakeSampleData[1] * kFakeScaleValue);
      EXPECT_EQ(update.get(source).z, kFakeSampleData[2] * kFakeScaleValue);
    }

    update_ = update;
  }

  std::optional<bool> is_supported_;
  AccelerometerUpdate update_;
};

}  // namespace

class AccelerometerProviderMojoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ash::AshTestHelper::InitParams init_params;
    init_params.start_session = false;
    ash_test_helper_.SetUp(std::move(init_params));

    sensor_hal_server_ =
        std::make_unique<chromeos::sensors::FakeSensorHalServer>();
    provider_ = new AccelerometerProviderMojo();

    chromeos::sensors::SensorHalDispatcher::Initialize();
    provider_->PrepareAndInitialize();
    provider_->AddObserver(&observer_);
  }

  void TearDown() override {
    chromeos::sensors::SensorHalDispatcher::Shutdown();
  }

  void AddDevice(int32_t iio_device_id,
                 chromeos::sensors::mojom::DeviceType type,
                 std::optional<std::string> scale,
                 std::optional<std::string> location) {
    std::set<chromeos::sensors::mojom::DeviceType> types;
    types.emplace(type);

    std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data;
    if (type == chromeos::sensors::mojom::DeviceType::ACCEL) {
      channels_data.resize(kNumberOfAxes);
      for (uint32_t i = 0; i < kNumberOfAxes; ++i) {
        channels_data[i].id = kAccelerometerChannels[i];
        channels_data[i].sample_data = kFakeSampleData[i];
      }
    }

    std::unique_ptr<chromeos::sensors::FakeSensorDevice> sensor_device(
        new chromeos::sensors::FakeSensorDevice(std::move(channels_data)));
    if (scale.has_value())
      sensor_device->SetAttribute(chromeos::sensors::mojom::kScale,
                                  scale.value());
    if (location.has_value())
      sensor_device->SetAttribute(chromeos::sensors::mojom::kLocation,
                                  location.value());

    sensor_devices_.emplace(iio_device_id, sensor_device.get());
    sensor_hal_server_->GetSensorService()->SetDevice(
        iio_device_id, std::move(types), std::move(sensor_device));
  }

  void AddLidAccelerometer() {
    AddDevice(kFakeLidAccelerometerId,
              chromeos::sensors::mojom::DeviceType::ACCEL,
              base::NumberToString(kFakeScaleValue),
              kLocationStrings[ACCELEROMETER_SOURCE_SCREEN]);
  }

  void TriggerNewDevicesTimeout() {
    provider_->OnNewDevicesTimeout();

    // Wait until task |OnECLidAngleDriverStatusChanged| arrives at |observer_|.
    base::RunLoop().RunUntilIdle();
  }

  void TriggerSamples() {
    // Simulate a disconnection of IIO Service.
    sensor_hal_server_->GetSensorService()->ClearReceivers();
    sensor_hal_server_->OnServerDisconnect();

    // Wait until the disconnect arrives at the dispatcher.
    base::RunLoop().RunUntilIdle();

    chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
        sensor_hal_server_->PassRemote());

    // Wait until a sample is received.
    base::RunLoop().RunUntilIdle();
  }

  FakeObserver observer_;
  std::unique_ptr<chromeos::sensors::FakeSensorHalServer> sensor_hal_server_;
  std::map<int32_t,
           raw_ptr<chromeos::sensors::FakeSensorDevice, CtnExperimental>>
      sensor_devices_;

  scoped_refptr<AccelerometerProviderMojo> provider_;

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};
  ash::AshTestHelper ash_test_helper_;
};

TEST_F(AccelerometerProviderMojoTest, CheckNoScale) {
  AddLidAccelerometer();
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL, std::nullopt,
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until all tasks done and no samples updated.
  base::RunLoop().RunUntilIdle();

  // Simulate timeout to check |ec_lid_angle_driver_status_|.
  TriggerNewDevicesTimeout();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID);
}

TEST_F(AccelerometerProviderMojoTest, CheckNoLocation) {
  AddLidAccelerometer();
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue), std::nullopt);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until all tasks done and no samples updated.
  base::RunLoop().RunUntilIdle();

  // Simulate timeout to check |ec_lid_angle_driver_status_|.
  TriggerNewDevicesTimeout();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID);
}

TEST_F(AccelerometerProviderMojoTest, GetSamplesOfOneAccel) {
  AddLidAccelerometer();
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  // Simulate timeout to check |ec_lid_angle_driver_status_|.
  TriggerNewDevicesTimeout();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));
}

TEST_F(AccelerometerProviderMojoTest, GetSamplesWithNoLidAngle) {
  AddLidAccelerometer();
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until samples are received.
  base::RunLoop().RunUntilIdle();

  // Simulate timeout to check |ec_lid_angle_driver_status_|.
  TriggerNewDevicesTimeout();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID_BASE);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  observer_.update_.Reset();

  // Simulate a disconnection of the accelerometer's mojo channel in IIO
  // Service.
  sensor_devices_[kFakeLidAccelerometerId]->ClearReceivers();

  // Wait until the disconnection is done.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sensor_hal_server_->GetSensorService()->HasReceivers());
}

TEST_F(AccelerometerProviderMojoTest, GetSamplesWithLidAngle) {
  AddLidAccelerometer();
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);
  AddDevice(kFakeLidAngleId, chromeos::sensors::mojom::DeviceType::ANGL,
            std::nullopt, std::nullopt);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until all setups are finished and no samples updated.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_TRUE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::ANGL_LID);
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  observer_.update_.Reset();

  provider_->TriggerRead();

  // Wait until samples are received.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  observer_.update_.Reset();

  // Simulate a disconnection of IIO Service.
  sensor_hal_server_->GetSensorService()->ClearReceivers();
  sensor_hal_server_->OnServerDisconnect();

  // Wait until the disconnect arrives at the dispatcher.
  base::RunLoop().RunUntilIdle();

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until samples are received.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));
}

TEST_F(AccelerometerProviderMojoTest, GetSamplesOfNewDevices) {
  // New device: lid-accelerometer.
  AddLidAccelerometer();
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  // Simulate timeout to check |ec_lid_angle_driver_status_|.
  TriggerNewDevicesTimeout();

  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID);

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  observer_.update_.Reset();

  // New device: base-accelerometer.
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);

  TriggerSamples();

  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID_BASE);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  observer_.update_.Reset();

  // New device: EC Lid Angle Driver.
  AddDevice(kFakeLidAngleId, chromeos::sensors::mojom::DeviceType::ANGL,
            std::nullopt, std::nullopt);

  TriggerSamples();

  EXPECT_TRUE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::ANGL_LID);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));
}

TEST_F(AccelerometerProviderMojoTest, NoSamplesFromBaseOnly) {
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(provider_->GetInitializationStateForTesting(),
            MojoState::INITIALIZING);

  EXPECT_FALSE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  // New device: base-accelerometer.
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);

  // Wait until all setups are finished and no samples updated.
  base::RunLoop().RunUntilIdle();

  // Simulate timeout to check |ec_lid_angle_driver_status_|.
  TriggerNewDevicesTimeout();

  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::BASE);

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  // New device: lid-accelerometer.
  AddLidAccelerometer();

  TriggerSamples();

  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID_BASE);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  observer_.update_.Reset();

  // New device: EC Lid Angle Driver.
  AddDevice(kFakeLidAngleId, chromeos::sensors::mojom::DeviceType::ANGL,
            std::nullopt, std::nullopt);

  TriggerSamples();

  EXPECT_TRUE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::ANGL_LID);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));
}

TEST_F(AccelerometerProviderMojoTest, NoSamplesFromLidAngle) {
  // New device: EC Lid Angle Driver.
  AddDevice(kFakeLidAngleId, chromeos::sensors::mojom::DeviceType::ANGL,
            std::nullopt, std::nullopt);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until all setups are finished and no samples updated.
  base::RunLoop().RunUntilIdle();

  provider_->TriggerRead();

  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::ANGL);

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_TRUE(observer_.is_supported_.value());
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  // New device: base-accelerometer.
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);

  // Wait until all setups are finished and no samples updated.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::ANGL);

  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  // New device: lid-accelerometer.
  AddLidAccelerometer();

  TriggerSamples();

  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::ANGL_LID);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));
}

TEST_F(AccelerometerProviderMojoTest, ResetStatesWithNoLidAngle) {
  AddLidAccelerometer();
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until samples are received.
  base::RunLoop().RunUntilIdle();

  // Simulate timeout to check |ec_lid_angle_driver_status_|.
  TriggerNewDevicesTimeout();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID_BASE);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  observer_.update_.Reset();

  // Simulate a disconnection of the base accelerometer's mojo channel in IIO
  // Service with the reason of DEVICE_REMOVED.
  sensor_devices_[kFakeBaseAccelerometerId]->ClearReceiversWithReason(
      chromeos::sensors::mojom::SensorDeviceDisconnectReason::DEVICE_REMOVED,
      "Device was removed");
  // Overwrite the base accelerometer in the iioservice.
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ANGLVEL, std::nullopt,
            std::nullopt);

  // Wait until the disconnection and the re-initialization are done.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), MojoState::LID);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));
}

}  // namespace ash
