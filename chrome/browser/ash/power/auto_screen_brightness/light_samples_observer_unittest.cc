// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/power/auto_screen_brightness/light_samples_observer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/power/auto_screen_brightness/fake_observer.h"
#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"
#include "chromeos/components/sensors/fake_sensor_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

constexpr int64_t kFakeSampleData = 50;
constexpr int64_t kFakeColorSampleData = 60;
constexpr char kIlluminanceColorChannels[][18] = {
    "illuminance_red", "illuminance_green", "illuminance_blue"};

}  // namespace

class LightSamplesObserverTest : public testing::Test {
 protected:
  void SetChannels(bool is_color_sensor) {
    std::vector<chromeos::sensors::FakeSensorDevice::ChannelData> channels_data;
    chromeos::sensors::FakeSensorDevice::ChannelData illuminance_data;

    illuminance_data.id = chromeos::sensors::mojom::kLightChannel;
    illuminance_data.sample_data = kFakeSampleData;
    channels_data.push_back(std::move(illuminance_data));

    if (is_color_sensor) {
      for (size_t i = 0; i < std::size(kIlluminanceColorChannels); ++i) {
        illuminance_data.id = kIlluminanceColorChannels[i];
        illuminance_data.sample_data = kFakeColorSampleData;
        channels_data.push_back(std::move(illuminance_data));
      }
    }

    sensor_device_ = std::make_unique<chromeos::sensors::FakeSensorDevice>(
        std::move(channels_data));
  }

  void SetProvider(mojo::Remote<chromeos::sensors::mojom::SensorDevice> light) {
    als_reader_ = std::make_unique<AlsReader>();
    observer_ = std::make_unique<LightSamplesObserver>(als_reader_.get(),
                                                       std::move(light));
    als_reader_->AddObserver(&fake_observer_);
  }

  void DisableIlluminanceChannel(mojo::ReceiverId id) {
    sensor_device_->SetChannelsEnabledWithId(id, {0}, false);
  }

  std::unique_ptr<chromeos::sensors::FakeSensorDevice> sensor_device_;
  FakeObserver fake_observer_;

  std::unique_ptr<LightSamplesObserver> observer_;
  std::unique_ptr<AlsReader> als_reader_;

  base::HistogramTester histogram_tester_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(LightSamplesObserverTest, MissingChannels) {
  sensor_device_ = std::make_unique<chromeos::sensors::FakeSensorDevice>(
      std::vector<chromeos::sensors::FakeSensorDevice::ChannelData>{});

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> light;
  sensor_device_->AddReceiver(light.BindNewPipeAndPassReceiver());
  SetProvider(std::move(light));

  // Wait until the mojo connection is reset.
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.DataError",
      static_cast<int>(DataError::kMojoSamplesObserver), 1);
  EXPECT_FALSE(sensor_device_->HasReceivers());
  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 0);
}

TEST_F(LightSamplesObserverTest, StartReadingTwiceError) {
  SetChannels(false);

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> light;
  sensor_device_->AddReceiver(light.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
      pending_remote;
  auto null_receiver = pending_remote.InitWithNewPipeAndPassReceiver();
  light->StartReadingSamples(std::move(pending_remote));

  SetProvider(std::move(light));

  // Wait until the mojo connection is reset.
  base::RunLoop().RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "AutoScreenBrightness.DataError",
      static_cast<int>(DataError::kMojoSamplesObserver), 1);
  EXPECT_FALSE(sensor_device_->HasReceivers());
  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 0);
}

TEST_F(LightSamplesObserverTest, GetSamplesWithoutColorChannels) {
  SetChannels(false);

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> light;
  auto id = sensor_device_->AddReceiver(light.BindNewPipeAndPassReceiver());
  SetProvider(std::move(light));

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(sensor_device_->HasReceivers());
  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 1);
  EXPECT_EQ(fake_observer_.ambient_light(), kFakeSampleData);

  DisableIlluminanceChannel(id);

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sensor_device_->HasReceivers());
  // The updated sample is not sent to |OnAmbientLightUpdated(lux)|.
  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 1);

  // Simulate a disconnection of the observer's mojo channel in IIO Service.
  sensor_device_->ResetObserverRemote(id);

  // Wait until the disconnection is done.
  base::RunLoop().RunUntilIdle();
  // OnObserverDisconnect shouldn't reset SensorDevice's mojo endpoint so that
  // LightProviderMojo can get the disconnection.
  EXPECT_TRUE(sensor_device_->HasReceivers());
}

TEST_F(LightSamplesObserverTest, GetSamplesWithColorChannels) {
  SetChannels(true);

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> light;
  sensor_device_->AddReceiver(light.BindNewPipeAndPassReceiver());
  SetProvider(std::move(light));

  // Wait until a sample is received.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(sensor_device_->HasReceivers());
  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 1);
  EXPECT_EQ(fake_observer_.ambient_light(), kFakeSampleData);

  base::flat_map<int32_t, int64_t> sample;
  sample[2] = kFakeColorSampleData;

  // Should ignore a sample without the target channel: illuminance.
  observer_->OnSampleUpdated(sample);

  EXPECT_EQ(fake_observer_.num_received_ambient_lights(), 1);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
