// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_LIGHT_SAMPLES_OBSERVER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_LIGHT_SAMPLES_OBSERVER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

// LightSamplesObserver waits for a light sensor's samples from IIO Service.
// When a sample is updated from IIO Service, it's sent to the AlsReader and
// further notifies observers.
class LightSamplesObserver
    : public chromeos::sensors::mojom::SensorDeviceSamplesObserver {
 public:
  LightSamplesObserver(AlsReader* als_reader,
                       mojo::Remote<chromeos::sensors::mojom::SensorDevice>
                           sensor_device_remote);
  LightSamplesObserver(const LightSamplesObserver&) = delete;
  LightSamplesObserver& operator=(const LightSamplesObserver&) = delete;
  ~LightSamplesObserver() override;

  // chromeos::sensors::mojom::SensorDeviceSamplesObserver overrides:
  void OnSampleUpdated(const base::flat_map<int32_t, int64_t>& sample) override;
  void OnErrorOccurred(
      chromeos::sensors::mojom::ObserverErrorType type) override;

 private:
  void Reset();

  void GetAllChannelIdsCallback(
      const std::vector<std::string>& iio_channel_ids);
  void StartReading();

  mojo::PendingRemote<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
  GetPendingRemote();

  void OnObserverDisconnect();
  void SetFrequency();
  void SetFrequencyCallback(double result_frequency);
  void SetChannelsEnabled();
  void SetChannelsEnabledCallback(const std::vector<int32_t>& failed_indices);

  raw_ptr<AlsReader, DanglingUntriaged> als_reader_;

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote_;

  mojo::Receiver<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
      receiver_{this};

  // Channel index of the target channel: "illuminance".
  std::optional<int32_t> channel_index_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LightSamplesObserver> weak_ptr_factory_{this};
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_LIGHT_SAMPLES_OBSERVER_H_
