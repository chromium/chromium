// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_samples_observer.h"

#include <memory>
#include <utility>

#include "ash/accelerometer/accelerometer_constants.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr int kFakeAccelerometerId = 1;

constexpr int64_t kFakeSampleData[] = {1, 2, 3};

constexpr double kFakeScaleValue = 10.0;

class AccelerometerSamplesObserverTest : public ::testing::Test {
 protected:
  void SetChannels(uint32_t num_of_axes) {
    CHECK_LE(num_of_axes, kNumberOfAxes);
    std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data(
        num_of_axes);
    for (uint32_t i = 0; i < num_of_axes; ++i) {
      channels_data[i].id = kAccelerometerChannels[i];
      channels_data[i].sample_data = kFakeSampleData[i];
    }

    sensor_device_ = std::make_unique<chromeos::sensors::FakeSensorDevice>(
        std::move(channels_data));
  }

  void SetObserver(
      mojo::Remote<chromeos::sensors::mojom::SensorDevice> accelerometer) {
    observer_ = std::make_unique<AccelerometerSamplesObserver>(
        kFakeAccelerometerId, std::move(accelerometer), kFakeScaleValue,
        base::BindRepeating(
            &AccelerometerSamplesObserverTest::OnSampleUpdatedCallback,
            base::Unretained(this)));
  }

  void OnSampleUpdatedCallback(int iio_device_id, std::vector<float> sample) {
    EXPECT_EQ(iio_device_id, kFakeAccelerometerId);
    EXPECT_EQ(sample.size(), kNumberOfAxes);
    for (uint32_t i = 0; i < kNumberOfAxes; ++i)
      EXPECT_EQ(sample[i], kFakeSampleData[i] * kFakeScaleValue);

    ++num_samples_;
  }

  void DisableFirstChannel(mojo::ReceiverId id) {
    sensor_device_->SetChannelsEnabledWithId(id, {0}, false);
  }

  std::unique_ptr<chromeos::sensors::FakeSensorDevice> sensor_device_;
  std::unique_ptr<AccelerometerSamplesObserver> observer_;

  int num_samples_ = 0;

  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_F(AccelerometerSamplesObserverTest, MissingChannels) {
  SetChannels(kNumberOfAxes - 1);

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> accelerometer;
  sensor_device_->AddReceiver(accelerometer.BindNewPipeAndPassReceiver());
  SetObserver(std::move(accelerometer));

  // Wait until the mojo connection is reset.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sensor_device_->HasReceivers());
}

TEST_F(AccelerometerSamplesObserverTest, StartReadingTwiceError) {
  SetChannels(kNumberOfAxes);

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> accelerometer;
  sensor_device_->AddReceiver(accelerometer.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
      pending_remote;
  auto null_receiver = pending_remote.InitWithNewPipeAndPassReceiver();
  accelerometer->StartReadingSamples(std::move(pending_remote));

  SetObserver(std::move(accelerometer));

  // Wait until the mojo connection is reset.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(sensor_device_->HasReceivers());
}

TEST_F(AccelerometerSamplesObserverTest, GetSamples) {
  SetChannels(kNumberOfAxes);

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> accelerometer;
  auto id =
      sensor_device_->AddReceiver(accelerometer.BindNewPipeAndPassReceiver());

  SetObserver(std::move(accelerometer));
  observer_->SetEnabled(true);

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(sensor_device_->HasReceivers());
  EXPECT_EQ(num_samples_, 1);

  DisableFirstChannel(id);

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(sensor_device_->HasReceivers());
  // The updated sample is not sent to |OnSampleUpdatedCallback|.
  EXPECT_EQ(num_samples_, 1);

  // Simulate a disconnection of the observer's mojo channel in IIO Service.
  sensor_device_->ResetObserverRemote(id);

  // Wait until the disconnection is done.
  base::RunLoop().RunUntilIdle();

  // OnObserverDisconnect shouldn't reset SensorDevice's mojo endpoint so that
  // AccelerometerProviderMojo can get the disconnection.
  EXPECT_TRUE(sensor_device_->HasReceivers());
}

}  // namespace

}  // namespace ash
