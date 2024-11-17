// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/sensor_info/sensor_provider.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "ash/accelerometer/accelerometer_constants.h"
#include "ash/sensor_info/sensor_types.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "chromeos/components/sensors/fake_sensor_hal_server.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::chromeos::sensors::mojom::DeviceType;

namespace {

constexpr double kFakeScaleValue = 10.0;

constexpr int kFakeLidAccelerometerId = 1;
constexpr int kFakeBaseAccelerometerId = 2;
constexpr int kFakeBaseGyroscopeId = 3;
constexpr int kFakeLidAngleId = 4;

constexpr int64_t kFakeSampleData[] = {1, 2, 3};

class FakeObserver : public SensorObserver {
 public:
  void OnSensorUpdated(const SensorUpdate& update) override {
    for (int index = 0; index < static_cast<int>(SensorType::kSensorTypeCount);
         ++index) {
      auto source = static_cast<SensorType>(index);
      if (!update.has(source)) {
        continue;
      }

      if (source != SensorType::kLidAngle) {
        EXPECT_DOUBLE_EQ(update.get(source)->x,
                         kFakeSampleData[0] * kFakeScaleValue);
        EXPECT_DOUBLE_EQ(update.get(source)->y,
                         kFakeSampleData[1] * kFakeScaleValue);
        EXPECT_DOUBLE_EQ(update.get(source)->z,
                         kFakeSampleData[2] * kFakeScaleValue);
      } else {
        EXPECT_DOUBLE_EQ(update.get(source)->x, kFakeSampleData[0]);
      }
    }
    update_ = update;
  }

  SensorUpdate update_;
};

}  // namespace

class SensorProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    chromeos::sensors::SensorHalDispatcher::Initialize();
    sensor_hal_server_ =
        std::make_unique<chromeos::sensors::FakeSensorHalServer>();
    provider_ = std::make_unique<SensorProvider>();
    provider_->AddObserver(&observer_);
  }

  void TearDown() override {
    chromeos::sensors::SensorHalDispatcher::Shutdown();
  }

  void AddDevice(int32_t iio_device_id,
                 std::set<DeviceType> types,
                 std::optional<std::string> scale,
                 std::optional<std::string> location) {
    std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data;
    int size = 0;
    if (base::Contains(types, DeviceType::ANGL)) {
      channels_data.resize(1);
      channels_data[0].id = "angl";
      channels_data[0].sample_data = 1;
      channels_data[0].attrs["raw"] = "1";
    }
    if (base::Contains(types, DeviceType::ACCEL)) {
      size += kNumberOfAxes;
      channels_data.resize(size);
      for (uint32_t i = 0; i < kNumberOfAxes; ++i) {
        channels_data[size - kNumberOfAxes + i].id = kAccelerometerChannels[i];
        channels_data[size - kNumberOfAxes + i].sample_data =
            kFakeSampleData[i];
      }
    }
    if (base::Contains(types, DeviceType::ANGLVEL)) {
      size += kNumberOfAxes;
      channels_data.resize(size);
      for (uint32_t i = 0; i < kNumberOfAxes; ++i) {
        channels_data[size - kNumberOfAxes + i].id = kGyroscopeChannels[i];
        channels_data[size - kNumberOfAxes + i].sample_data =
            kFakeSampleData[i];
      }
    }

    std::unique_ptr<chromeos::sensors::FakeSensorDevice> sensor_device(
        std::make_unique<chromeos::sensors::FakeSensorDevice>(
            std::move(channels_data)));
    if (scale.has_value()) {
      sensor_device->SetAttribute(chromeos::sensors::mojom::kScale,
                                  scale.value());
    }
    if (location.has_value()) {
      sensor_device->SetAttribute(chromeos::sensors::mojom::kLocation,
                                  location.value());
    }
    sensor_devices_[iio_device_id] = sensor_device.get();
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor;
    sensor_device->AddReceiver(sensor.BindNewPipeAndPassReceiver());
    sensor_hal_server_->GetSensorService()->SetDevice(
        iio_device_id, std::move(types), std::move(sensor_device));
  }

  void StartConnection() {
    chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterServer(
        sensor_hal_server_->PassRemote());
  }

  base::test::SingleThreadTaskEnvironment task_environment;

  FakeObserver observer_;
  std::unique_ptr<chromeos::sensors::FakeSensorHalServer> sensor_hal_server_;
  std::map<int32_t,
           raw_ptr<chromeos::sensors::FakeSensorDevice, CtnExperimental>>
      sensor_devices_;
  std::unique_ptr<SensorProvider> provider_;
};

TEST_F(SensorProviderTest, CheckNoScale) {
  AddDevice(kFakeBaseAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            std::nullopt, kLocationStrings[1]);
  StartConnection();
  provider_->EnableSensorReading();
  // Wait until all tasks done and no samples updated.
  base::RunLoop().RunUntilIdle();

  // This vector is the state vector for {LidAngle, AccelerometerBase,
  // AccelerometerLid, GyroscopeBase, GyroscopeLid} sensors, where true
  // meaning the sensor detected and false means the sensor not detected.
  std::vector<bool> expected{false, false, false, false, false};

  EXPECT_EQ(provider_->GetStateForTesting(), expected);
  EXPECT_FALSE(observer_.update_.has(SensorType::kAccelerometerBase));
}

TEST_F(SensorProviderTest, CheckNoLocation) {
  AddDevice(kFakeBaseAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            base::NumberToString(kFakeScaleValue), std::nullopt);

  StartConnection();
  provider_->EnableSensorReading();
  // Wait until all tasks done and no samples updated.
  base::RunLoop().RunUntilIdle();

  // This vector is the state vector for {LidAngle, AccelerometerBase,
  // AccelerometerLid, GyroscopeBase, GyroscopeLid} sensors, where true
  // meaning the sensor detected and false means the sensor not detected.
  std::vector<bool> expected{false, false, false, false, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected);
  EXPECT_FALSE(observer_.update_.has(SensorType::kAccelerometerLid));
  EXPECT_FALSE(observer_.update_.has(SensorType::kAccelerometerBase));
}

TEST_F(SensorProviderTest, GetSamplesOfLidAccel) {
  AddDevice(kFakeLidAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[0]);
  StartConnection();
  provider_->EnableSensorReading();
  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  // This vector is the state vector for {LidAngle, AccelerometerBase,
  // AccelerometerLid, GyroscopeBase, GyroscopeLid} sensors, where true
  // meaning the sensor detected and false means the sensor not detected.
  std::vector<bool> expected{false, false, true, false, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected);
  EXPECT_TRUE(sensor_hal_server_->GetSensorService()->HasReceivers());
  EXPECT_TRUE(base::Contains(sensor_devices_, kFakeLidAccelerometerId));
  EXPECT_TRUE(sensor_devices_[kFakeLidAccelerometerId]->HasReceivers());
  EXPECT_TRUE(observer_.update_.has(SensorType::kAccelerometerLid));
}

TEST_F(SensorProviderTest, GetSamplesOfLidAngleAndLidAccel) {
  AddDevice(kFakeLidAngleId, std::set<DeviceType>{DeviceType::ANGL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[0]);
  // LidAngle sample update is generated by reading a sensor property, which
  // causes endless SensorsSamples updates if LidAngle is the only present
  // sensor. It is OK in production but not good for this test. So we add a
  // AccelerometerLid to let SensorProvider generate only one update.
  AddDevice(kFakeLidAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[0]);
  StartConnection();
  provider_->EnableSensorReading();
  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  // This vector is the state vector for {LidAngle, AccelerometerBase,
  // AccelerometerLid, GyroscopeBase, GyroscopeLid} sensors, where true
  // meaning the sensor detected and false means the sensor not detected.
  std::vector<bool> expected{true, false, true, false, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected);
  EXPECT_TRUE(observer_.update_.has(SensorType::kLidAngle));
  EXPECT_TRUE(observer_.update_.has(SensorType::kAccelerometerLid));
}

TEST_F(SensorProviderTest, GetSamplesOfBaseGyroscope) {
  AddDevice(kFakeBaseGyroscopeId, std::set<DeviceType>{DeviceType::ANGLVEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[1]);

  StartConnection();
  provider_->EnableSensorReading();
  base::RunLoop().RunUntilIdle();

  // This vector is the state vector for {LidAngle, AccelerometerBase,
  // AccelerometerLid, GyroscopeBase, GyroscopeLid} sensors, where true
  // meaning the sensor detected and false means the sensor not detected.
  std::vector<bool> expected{false, false, false, true, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected);
  EXPECT_TRUE(observer_.update_.has(SensorType::kGyroscopeBase));
}

TEST_F(SensorProviderTest, GetSamplesOfBaseGyroscopeAndBaseAccel) {
  // Creates device: GyroscopeBase and AccelerometerBase.
  AddDevice(kFakeBaseGyroscopeId, std::set<DeviceType>{DeviceType::ANGLVEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[1]);
  AddDevice(kFakeBaseAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[1]);

  StartConnection();
  provider_->EnableSensorReading();
  base::RunLoop().RunUntilIdle();

  // This vector is the state vector for {LidAngle, AccelerometerBase,
  // AccelerometerLid, GyroscopeBase, GyroscopeLid} sensors, where true
  // meaning the sensor detected and false means the sensor not detected.
  std::vector<bool> expected1{false, true, false, true, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected1);
  EXPECT_TRUE(observer_.update_.has(SensorType::kAccelerometerBase));
  EXPECT_TRUE(observer_.update_.has(SensorType::kGyroscopeBase));

  // Triggering SensorProvider::OnSensorServiceDisconnect.
  sensor_hal_server_->GetSensorService()->ClearReceivers();
  sensor_hal_server_->OnServerDisconnect();
  // Wait until the disconnect arrives at the dispatcher.
  base::RunLoop().RunUntilIdle();
  // Overwriting with invalid AccelerometerBase.
  AddDevice(kFakeBaseAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            std::nullopt, std::nullopt);
  StartConnection();
  // Wait until the re-initialization done.
  base::RunLoop().RunUntilIdle();
  std::vector<bool> expected2{false, false, false, true, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected2);
  EXPECT_FALSE(observer_.update_.has(SensorType::kAccelerometerBase));
  EXPECT_TRUE(observer_.update_.has(SensorType::kGyroscopeBase));
}

TEST_F(SensorProviderTest, AddSensorsWhileSampling) {
  StartConnection();
  provider_->EnableSensorReading();
  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  // This vector is the state vector for {LidAngle, AccelerometerBase,
  // AccelerometerLid, GyroscopeBase, GyroscopeLid} sensors, where true
  // meaning the sensor detected and false means the sensor not detected.
  std::vector<bool> expected1{false, false, false, false, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected1);

  // New device: AccelerometerBase.
  AddDevice(kFakeBaseAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[1]);

  // Wait until all setups are finished and no samples updated.
  base::RunLoop().RunUntilIdle();

  std::vector<bool> expected2{false, true, false, false, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected2);
  EXPECT_TRUE(observer_.update_.has(SensorType::kAccelerometerBase));

  // New device: AccelerometerLid. Disconnect AccelerometerBase and reconnect.
  AddDevice(kFakeLidAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[0]);
  // Simulate a disconnection of the base accelerometer's mojo channel in IIO
  // Service with the reason of DEVICE_REMOVED.
  sensor_devices_[kFakeBaseAccelerometerId]->ClearReceiversWithReason(
      chromeos::sensors::mojom::SensorDeviceDisconnectReason::DEVICE_REMOVED,
      "Device was removed");
  // Overwrite the AccelerometerBase with valid device.
  AddDevice(kFakeBaseAccelerometerId, std::set<DeviceType>{DeviceType::ACCEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[1]);

  // Wait until the disconnection and the re-initialization are done.
  base::RunLoop().RunUntilIdle();

  std::vector<bool> expected3{false, true, true, false, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected3);
  EXPECT_TRUE(observer_.update_.has(SensorType::kAccelerometerLid));
  EXPECT_TRUE(observer_.update_.has(SensorType::kAccelerometerBase));
}

TEST_F(SensorProviderTest, GetSamplesOfAccelGyroDevices) {
  // New device: GyroscopeBase and AccelerometerBase.
  AddDevice(kFakeBaseGyroscopeId,
            std::set<DeviceType>{DeviceType::ACCEL, DeviceType::ANGLVEL},
            base::NumberToString(kFakeScaleValue), kLocationStrings[1]);

  StartConnection();
  provider_->EnableSensorReading();
  base::RunLoop().RunUntilIdle();

  // This vector is the state vector for {LidAngle, AccelerometerBase,
  // AccelerometerLid, GyroscopeBase, GyroscopeLid} sensors, where true
  // meaning the sensor detected and false means the sensor not detected.
  std::vector<bool> expected{false, true, false, true, false};
  EXPECT_EQ(provider_->GetStateForTesting(), expected);
  EXPECT_TRUE(observer_.update_.has(SensorType::kAccelerometerBase));
  EXPECT_TRUE(observer_.update_.has(SensorType::kGyroscopeBase));
}

}  // namespace ash
