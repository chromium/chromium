// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_samples_observer.h"

#include <utility>

#include "base/bind.h"

namespace ash {

namespace {

constexpr int kTimeoutToleranceInMilliseconds = 500;
constexpr double kReadFrequencyInHz = 10.0;

}  // namespace

AccelerometerSamplesObserver::AccelerometerSamplesObserver(
    int iio_device_id,
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote,
    float scale,
    OnSampleUpdatedCallback on_sample_updated_callback)
    : iio_device_id_(iio_device_id),
      sensor_device_remote_(std::move(sensor_device_remote)),
      scale_(scale),
      on_sample_updated_callback_(std::move(on_sample_updated_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());

  sensor_device_remote_->GetAllChannelIds(
      base::BindOnce(&AccelerometerSamplesObserver::GetAllChannelIdsCallback,
                     weak_factory_.GetWeakPtr()));
}

AccelerometerSamplesObserver::~AccelerometerSamplesObserver() = default;

void AccelerometerSamplesObserver::SetEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (enabled_ == enabled)
    return;

  enabled_ = enabled;

  UpdateSensorDeviceFrequency();
}

void AccelerometerSamplesObserver::OnSampleUpdated(
    const base::flat_map<int32_t, int64_t>& sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sample.size() != kNumberOfAxes) {
    LOG(ERROR) << "Invalid sample with size: " << sample.size();
    OnErrorOccurred(chromeos::sensors::mojom::ObserverErrorType::READ_FAILED);
    return;
  }

  auto it = sample.begin();
  std::vector<float> output_sample;
  for (size_t axes = 0; axes < kNumberOfAxes; ++axes) {
    if (axes != 0)
      ++it;

    if (it == sample.end() || it->first != channel_indices_[axes])
      it = sample.find(channel_indices_[axes]);

    if (it == sample.end()) {
      LOG(ERROR) << "Missing channel: " << kAccelerometerChannels[axes]
                 << " in sample";
      OnErrorOccurred(chromeos::sensors::mojom::ObserverErrorType::READ_FAILED);
      return;
    }

    output_sample.push_back(it->second * scale_);
  }

  on_sample_updated_callback_.Run(iio_device_id_, output_sample);
}

void AccelerometerSamplesObserver::OnErrorOccurred(
    chromeos::sensors::mojom::ObserverErrorType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (type) {
    case chromeos::sensors::mojom::ObserverErrorType::ALREADY_STARTED:
      LOG(ERROR) << "Device " << iio_device_id_
                 << ": Another observer has already started to read samples";
      Reset();
      break;

    case chromeos::sensors::mojom::ObserverErrorType::FREQUENCY_INVALID:
      if (!enabled_)  // It's normal if this observer is not enabled
        break;

      LOG(ERROR) << "Device " << iio_device_id_
                 << ": Observer started with an invalid frequency";
      UpdateSensorDeviceFrequency();

      break;

    case chromeos::sensors::mojom::ObserverErrorType::NO_ENABLED_CHANNELS:
      LOG(ERROR) << "Device " << iio_device_id_
                 << ": Observer started with no channels enabled";
      if (sensor_device_remote_.is_bound()) {
        sensor_device_remote_->SetChannelsEnabled(
            std::vector<int32_t>(channel_indices_,
                                 channel_indices_ + kNumberOfAxes),
            /*enable=*/true,
            base::BindOnce(
                &AccelerometerSamplesObserver::SetChannelsEnabledCallback,
                weak_factory_.GetWeakPtr()));
      }

      break;

    case chromeos::sensors::mojom::ObserverErrorType::SET_FREQUENCY_IO_FAILED:
      LOG(ERROR) << "Device " << iio_device_id_
                 << ": Failed to set frequency to the physical device";
      break;

    case chromeos::sensors::mojom::ObserverErrorType::GET_FD_FAILED:
      LOG(ERROR) << "Device " << iio_device_id_
                 << ": Failed to get the device's fd to poll on";
      break;

    case chromeos::sensors::mojom::ObserverErrorType::READ_FAILED:
      LOG(ERROR) << "Device " << iio_device_id_ << ": Failed to read a sample";
      break;

    case chromeos::sensors::mojom::ObserverErrorType::READ_TIMEOUT:
      LOG(ERROR) << "Device " << iio_device_id_ << ": A read timed out";
      break;

    default:
      LOG(ERROR) << "Device " << iio_device_id_ << ": error "
                 << static_cast<int>(type);
      break;
  }
}

void AccelerometerSamplesObserver::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "Resetting AccelerometerSamplesObserver: " << iio_device_id_;
  receiver_.reset();
  sensor_device_remote_.reset();
}

void AccelerometerSamplesObserver::GetAllChannelIdsCallback(
    const std::vector<std::string>& iio_channel_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());

  iio_channel_ids_ = std::move(iio_channel_ids);

  for (size_t axis = 0; axis < kNumberOfAxes; ++axis) {
    bool found = false;
    for (size_t channel_index = 0; channel_index < iio_channel_ids_.size();
         ++channel_index) {
      if (iio_channel_ids_[channel_index].compare(
              kAccelerometerChannels[axis]) == 0) {
        found = true;
        channel_indices_[axis] = channel_index;
        break;
      }
    }

    if (!found) {
      LOG(ERROR) << "Missing channel: " << kAccelerometerChannels[axis];
      Reset();
      return;
    }
  }

  sensor_device_remote_->SetChannelsEnabled(
      std::vector<int32_t>(channel_indices_, channel_indices_ + kNumberOfAxes),
      /*enable=*/true,
      base::BindOnce(&AccelerometerSamplesObserver::SetChannelsEnabledCallback,
                     weak_factory_.GetWeakPtr()));

  StartReading();
}

void AccelerometerSamplesObserver::StartReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());

  sensor_device_remote_->SetTimeout(kTimeoutToleranceInMilliseconds);

  sensor_device_remote_->StartReadingSamples(GetPendingRemote());
}

void AccelerometerSamplesObserver::UpdateSensorDeviceFrequency() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sensor_device_remote_.is_bound())
    return;

  sensor_device_remote_->SetFrequency(
      enabled_ ? kReadFrequencyInHz : 0.0,
      base::BindOnce(&AccelerometerSamplesObserver::SetFrequencyCallback,
                     weak_factory_.GetWeakPtr(), enabled_));
}

mojo::PendingRemote<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
AccelerometerSamplesObserver::GetPendingRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto pending_remote = receiver_.BindNewPipeAndPassRemote();

  receiver_.set_disconnect_handler(
      base::BindOnce(&AccelerometerSamplesObserver::OnObserverDisconnect,
                     weak_factory_.GetWeakPtr()));
  return pending_remote;
}

void AccelerometerSamplesObserver::OnObserverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "OnObserverDisconnect error, assuming IIO Service crashes and "
                "waiting for its relaunch.";
  // Don't reset |sensor_device_remote_| so that AccelerometerProviderMojo can
  // get the disconnection.
  receiver_.reset();
}

void AccelerometerSamplesObserver::SetFrequencyCallback(
    bool enabled,
    double result_frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (enabled != enabled_) {
    // As the current configuration (required frequency) is different now,
    // ignore the result of this deprecated |SensorDevice::SetFrequency|.
    return;
  }

  if ((result_frequency > 0.0 && enabled_) ||
      (result_frequency == 0.0 && !enabled_)) {
    return;
  }

  LOG(ERROR) << "Failed to set frequency: " << result_frequency
             << " with the samples observer enabled: "
             << (enabled_ ? "true" : "false");
  Reset();
}

void AccelerometerSamplesObserver::SetChannelsEnabledCallback(
    const std::vector<int32_t>& failed_indices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (int32_t index : failed_indices)
    LOG(ERROR) << "Failed to enable " << iio_channel_ids_[index];

  if (!failed_indices.empty())
    Reset();
}

}  // namespace ash
