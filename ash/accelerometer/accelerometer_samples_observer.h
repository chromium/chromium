// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_SAMPLES_OBSERVER_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_SAMPLES_OBSERVER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "ash/accelerometer/accelerometer_constants.h"
#include "ash/ash_export.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// A SamplesObserver for an accelerometer device. When a sample is updated from
// IIO Service, it's sent to the AccelerometerProviderMojo via the callback
// |on_sample_udpated_callback_| registered in the constructor.
// AccelerometerSamplesObserver should only be used on the UI thread.
class ASH_EXPORT AccelerometerSamplesObserver
    : public chromeos::sensors::mojom::SensorDeviceSamplesObserver {
 public:
  using OnSampleUpdatedCallback =
      base::RepeatingCallback<void(int iio_device_id,
                                   std::vector<float> sample)>;

  AccelerometerSamplesObserver(
      int iio_device_id,
      mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote,
      float scale,
      OnSampleUpdatedCallback on_sample_updated_callback);
  AccelerometerSamplesObserver(const AccelerometerSamplesObserver&) = delete;
  AccelerometerSamplesObserver& operator=(const AccelerometerSamplesObserver&) =
      delete;
  ~AccelerometerSamplesObserver() override;

  // Sets the observer |enabled| by setting the frequency to iioservice.
  // Should be called on |task_runner_|.
  void SetEnabled(bool enabled);

  // chromeos::sensors::mojom::SensorDeviceSamplesObserver overrides:
  void OnSampleUpdated(const base::flat_map<int32_t, int64_t>& sample) override;
  void OnErrorOccurred(
      chromeos::sensors::mojom::ObserverErrorType type) override;

 private:
  void Reset();

  void GetAllChannelIdsCallback(
      const std::vector<std::string>& iio_channel_ids);
  void StartReading();

  // Update this sensor device's frequency to kReadFrequencyInHz if |enabled_|
  // is true, and to 0 if |enabled_| is false.
  void UpdateSensorDeviceFrequency();

  mojo::PendingRemote<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
  GetPendingRemote();

  void OnObserverDisconnect();
  void SetFrequencyCallback(bool enabled, double result_frequency);
  void SetChannelsEnabledCallback(const std::vector<int32_t>& failed_indices);

  int iio_device_id_;
  mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote_;

  double scale_;

  // Callback to send samples to the owner of this class.
  OnSampleUpdatedCallback on_sample_updated_callback_;

  // Boolean to indicate if this accelerometer should set a valid frequency and
  // keep reading samples.
  bool enabled_ = false;

  // The list of channel ids retrieved from iioservice. Use channels' indices
  // in this list to identify them.
  std::vector<std::string> iio_channel_ids_;
  // Channel indices (of accel_x, accel_y, and accel_z respectively) to
  // enable.
  int32_t channel_indices_[kNumberOfAxes];

  mojo::Receiver<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
      receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AccelerometerSamplesObserver> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_SAMPLES_OBSERVER_H_
