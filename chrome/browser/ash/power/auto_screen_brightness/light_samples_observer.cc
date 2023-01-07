// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/light_samples_observer.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

LightSamplesObserver::LightSamplesObserver(
    AlsReader* als_reader,
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote)
    : als_reader_(als_reader),
      sensor_device_remote_(std::move(sensor_device_remote)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());

  sensor_device_remote_->GetAllChannelIds(
      base::BindOnce(&LightSamplesObserver::GetAllChannelIdsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

LightSamplesObserver::~LightSamplesObserver() = default;

void LightSamplesObserver::OnSampleUpdated(
    const base::flat_map<int32_t, int64_t>& sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(channel_index_.has_value());
  DCHECK(als_reader_);

  const auto it = sample.find(channel_index_.value());
  if (it == sample.end()) {
    LogDataError(DataError::kAlsValue);
    return;
  }

  als_reader_->SetLux(it->second);
}

void LightSamplesObserver::OnErrorOccurred(
    chromeos::sensors::mojom::ObserverErrorType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (type) {
    case chromeos::sensors::mojom::ObserverErrorType::ALREADY_STARTED:
      LOG(ERROR) << "Another observer has already started to read samples";
      Reset();
      break;

    case chromeos::sensors::mojom::ObserverErrorType::FREQUENCY_INVALID:
      LOG(ERROR) << "Observer started with an invalid frequency";
      SetFrequency();
      break;

    case chromeos::sensors::mojom::ObserverErrorType::NO_ENABLED_CHANNELS:
      LOG(ERROR) << "Observer started with no channels enabled";
      SetChannelsEnabled();
      break;

    case chromeos::sensors::mojom::ObserverErrorType::SET_FREQUENCY_IO_FAILED:
      LOG(ERROR) << "Failed to set frequency to the physical device";
      break;

    case chromeos::sensors::mojom::ObserverErrorType::GET_FD_FAILED:
      LOG(ERROR) << "Failed to get the device's fd to poll on";
      break;

    case chromeos::sensors::mojom::ObserverErrorType::READ_FAILED:
      LOG(ERROR) << "Failed to read a sample";
      break;

    case chromeos::sensors::mojom::ObserverErrorType::READ_TIMEOUT:
      LOG(ERROR) << "A read timed out";
      break;

    default:
      LOG(ERROR) << "Error: " << static_cast<int>(type);
      break;
  }
}

void LightSamplesObserver::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(als_reader_);

  LOG(ERROR) << "Resetting LightSamplesObserver";
  receiver_.reset();
  sensor_device_remote_.reset();

  LogDataError(DataError::kMojoSamplesObserver);
}

void LightSamplesObserver::GetAllChannelIdsCallback(
    const std::vector<std::string>& iio_channel_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());

  for (uint32_t i = 0; i < iio_channel_ids.size(); ++i) {
    if (iio_channel_ids[i].compare(chromeos::sensors::mojom::kLightChannel) ==
        0) {
      channel_index_ = i;
      break;
    }
  }

  if (!channel_index_.has_value()) {
    LOG(ERROR) << "Missing the available lux channel";
    Reset();
    return;
  }

  StartReading();
}

void LightSamplesObserver::StartReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_device_remote_.is_bound());

  sensor_device_remote_->SetTimeout(0);
  SetFrequency();
  SetChannelsEnabled();

  sensor_device_remote_->StartReadingSamples(GetPendingRemote());
}

mojo::PendingRemote<chromeos::sensors::mojom::SensorDeviceSamplesObserver>
LightSamplesObserver::GetPendingRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto pending_remote = receiver_.BindNewPipeAndPassRemote();

  receiver_.set_disconnect_handler(
      base::BindOnce(&LightSamplesObserver::OnObserverDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  return pending_remote;
}

void LightSamplesObserver::OnObserverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "OnObserverDisconnect";

  // Don't reset |sensor_device_remote_| so that LightProviderMojo can get the
  // disconnection.
  receiver_.reset();
}

void LightSamplesObserver::SetFrequency() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sensor_device_remote_->SetFrequency(
      AlsReader::kAlsPollFrequency,
      base::BindOnce(&LightSamplesObserver::SetFrequencyCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LightSamplesObserver::SetFrequencyCallback(double result_frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result_frequency > 0.0)
    return;

  LOG(ERROR) << "Failed to set frequency: " << AlsReader::kAlsPollFrequency;
  Reset();
}

void LightSamplesObserver::SetChannelsEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(channel_index_.has_value());

  sensor_device_remote_->SetChannelsEnabled(
      std::vector<int32_t>{channel_index_.value()},
      /*enable=*/true,
      base::BindOnce(&LightSamplesObserver::SetChannelsEnabledCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LightSamplesObserver::SetChannelsEnabledCallback(
    const std::vector<int32_t>& failed_indices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const int32_t index : failed_indices) {
    if (index == channel_index_) {
      LOG(ERROR) << "Failed to enable the lux channel: "
                 << channel_index_.value();
      Reset();
      return;
    }

    LOG(WARNING) << "Failed to enable an unnecessary channel with index: "
                 << index;
  }
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
