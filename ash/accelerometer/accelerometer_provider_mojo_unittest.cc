// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_provider_mojo.h"

#include <memory>
#include <utility>

#include "ash/accelerometer/accelerometer_constants.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "chromeos/components/sensors/fake_sensor_hal_server.h"
#include "chromeos/components/sensors/sensor_hal_dispatcher.h"
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
    CHECK(!is_supported_.has_value());
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

  base::Optional<bool> is_supported_;
  AccelerometerUpdate update_;
};

class AccelerometerProviderMojoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    sensor_hal_server_ =
        std::make_unique<chromeos::sensors::FakeSensorHalServer>();
    provider_ = new AccelerometerProviderMojo();

    chromeos::sensors::SensorHalDispatcher::Initialize();
    provider_->PrepareAndInitialize();
    provider_->AddObserver(&observer_);

    AddDevice(kFakeLidAccelerometerId,
              chromeos::sensors::mojom::DeviceType::ACCEL,
              base::NumberToString(kFakeScaleValue),
              kLocationStrings[ACCELEROMETER_SOURCE_SCREEN]);
  }

  void TearDown() override {
    chromeos::sensors::SensorHalDispatcher::Shutdown();
  }

  void AddDevice(int32_t iio_device_id,
                 chromeos::sensors::mojom::DeviceType type,
                 base::Optional<std::string> scale,
                 base::Optional<std::string> location) {
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

    sensor_hal_server_->GetSensorService()->SetDevice(
        iio_device_id, std::move(types), std::move(sensor_device));
  }

  FakeObserver observer_;
  std::unique_ptr<chromeos::sensors::FakeSensorHalServer> sensor_hal_server_;
  scoped_refptr<AccelerometerProviderMojo> provider_;

  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(AccelerometerProviderMojoTest, CheckNoScale) {
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL, base::nullopt,
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until initialization failed.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), State::FAILED);
}

TEST_F(AccelerometerProviderMojoTest, CheckNoLocation) {
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue), base::nullopt);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until initialization failed.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), State::SUCCESS);
}

TEST_F(AccelerometerProviderMojoTest, GetSamplesOfOneAccel) {
  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), State::SUCCESS);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_FALSE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));
}

TEST_F(AccelerometerProviderMojoTest, GetSamplesWithNoLidAngle) {
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until samples are received.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_FALSE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), State::SUCCESS);

  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_SCREEN));
  EXPECT_TRUE(observer_.update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD));

  observer_.update_.Reset();

  // Simulate a disconnection of the accelerometer's mojo channel in IIO
  // Service.
  AddDevice(kFakeLidAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_SCREEN]);

  // Wait until the disconnection is done.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sensor_hal_server_->GetSensorService()->is_bound());
}

TEST_F(AccelerometerProviderMojoTest, GetSamplesWithLidAngle) {
  AddDevice(kFakeBaseAccelerometerId,
            chromeos::sensors::mojom::DeviceType::ACCEL,
            base::NumberToString(kFakeScaleValue),
            kLocationStrings[ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD]);
  AddDevice(kFakeLidAngleId, chromeos::sensors::mojom::DeviceType::ANGL,
            base::nullopt, base::nullopt);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
      sensor_hal_server_->PassRemote());

  // Wait until all setups are finished and no samples updated.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(observer_.is_supported_.has_value());
  EXPECT_TRUE(observer_.is_supported_.value());
  EXPECT_EQ(provider_->GetInitializationStateForTesting(), State::SUCCESS);
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
  sensor_hal_server_->GetSensorService()->OnServiceDisconnect();
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

}  // namespace

}  // namespace ash
