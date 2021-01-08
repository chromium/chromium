// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_provider_mojo.h"

#include <iterator>
#include <utility>

#include "ash/accelerometer/accelerometer_constants.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/components/sensors/sensor_hal_dispatcher.h"

namespace ash {

namespace {

// Delay of the reconnection to Sensor Hal Dispatcher.
constexpr base::TimeDelta kDelayReconnect =
    base::TimeDelta::FromMilliseconds(1000);

}  // namespace

AccelerometerProviderMojo::AccelerometerProviderMojo() = default;

void AccelerometerProviderMojo::PrepareAndInitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RegisterSensorClient();
}

void AccelerometerProviderMojo::TriggerRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::SUPPORTED)
    EnableAccelerometerReading();
}

void AccelerometerProviderMojo::CancelRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::SUPPORTED)
    DisableAccelerometerReading();
}

void AccelerometerProviderMojo::SetUpChannel(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
        pending_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sensor_service_remote_.is_bound()) {
    LOG(ERROR) << "Ignoring the second Remote<SensorService>";
    return;
  }

  sensor_service_remote_.Bind(std::move(pending_remote));
  sensor_service_remote_.set_disconnect_handler(base::BindOnce(
      &AccelerometerProviderMojo::OnSensorServiceDisconnect, this));
  if (GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::UNKNOWN) {
    sensor_service_remote_->GetDeviceIds(
        chromeos::sensors::mojom::DeviceType::ANGL,
        base::BindOnce(&AccelerometerProviderMojo::GetLidAngleIdsCallback,
                       this));
  }

  sensor_service_remote_->GetDeviceIds(
      chromeos::sensors::mojom::DeviceType::ACCEL,
      base::BindOnce(&AccelerometerProviderMojo::GetAccelerometerIdsCallback,
                     this));
}

State AccelerometerProviderMojo::GetInitializationStateForTesting() const {
  return initialization_state_;
}

bool AccelerometerProviderMojo::ShouldDelayOnTabletPhysicalStateChanged() {
  if (GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::UNKNOWN) {
    pending_on_tablet_physical_state_changed_ = true;
    return true;
  }

  return false;
}

AccelerometerProviderMojo::AccelerometerData::AccelerometerData() = default;
AccelerometerProviderMojo::AccelerometerData::~AccelerometerData() = default;

AccelerometerProviderMojo::~AccelerometerProviderMojo() = default;

void AccelerometerProviderMojo::RegisterSensorClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterClient(
      sensor_hal_client_.BindNewPipeAndPassRemote());

  sensor_hal_client_.set_disconnect_handler(base::BindOnce(
      &AccelerometerProviderMojo::OnSensorHalClientFailure, this));
}

void AccelerometerProviderMojo::OnSensorHalClientFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "OnSensorHalClientFailure";

  ResetSensorService();
  sensor_hal_client_.reset();

  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AccelerometerProviderMojo::RegisterSensorClient, this),
      kDelayReconnect);
}

void AccelerometerProviderMojo::OnSensorServiceDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "OnSensorServiceDisconnect";

  ResetSensorService();
}

void AccelerometerProviderMojo::ResetSensorService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& accelerometer : accelerometers_) {
    accelerometer.second.remote.reset();
    accelerometer.second.samples_observer.reset();
  }
  sensor_service_remote_.reset();
}

void AccelerometerProviderMojo::GetLidAngleIdsCallback(
    const std::vector<int32_t>& lid_angle_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetECLidAngleDriverStatus(), ECLidAngleDriverStatus::UNKNOWN);

  if (!lid_angle_ids.empty()) {
    SetECLidAngleDriverStatus(ECLidAngleDriverStatus::SUPPORTED);
  } else {
    SetECLidAngleDriverStatus(ECLidAngleDriverStatus::NOT_SUPPORTED);
    EnableAccelerometerReading();
  }

  if (pending_on_tablet_physical_state_changed_)
    OnTabletPhysicalStateChanged();
}

void AccelerometerProviderMojo::GetAccelerometerIdsCallback(
    const std::vector<int32_t>& accelerometer_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (accelerometer_ids.empty()) {
    FailedToInitialize();
    return;
  }

  for (int32_t id : accelerometer_ids)
    RegisterAccelerometerWithId(id);
}

void AccelerometerProviderMojo::RegisterAccelerometerWithId(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& accelerometer = accelerometers_[id];

  if (accelerometer.ignored) {
    // Something went wrong in the previous initialization. Ignoring this accel.
    return;
  }

  DCHECK(!accelerometer.remote.is_bound());
  DCHECK(!accelerometer.samples_observer.get());

  if (!sensor_service_remote_.is_bound()) {
    // Something went wrong. Skipping here.
    return;
  }

  accelerometer.remote.reset();

  sensor_service_remote_->GetDevice(
      id, accelerometer.remote.BindNewPipeAndPassReceiver());
  accelerometer.remote.set_disconnect_handler(base::BindOnce(
      &AccelerometerProviderMojo::OnAccelerometerRemoteDisconnect, this, id));

  std::vector<std::string> attr_names;
  if (!accelerometer.location.has_value())
    attr_names.push_back(chromeos::sensors::mojom::kLocation);
  if (!accelerometer.scale.has_value())
    attr_names.push_back(chromeos::sensors::mojom::kScale);

  if (!attr_names.empty()) {
    accelerometer.remote->GetAttributes(
        attr_names,
        base::BindOnce(&AccelerometerProviderMojo::GetAttributesCallback, this,
                       id));
  } else {
    // Create the observer directly if the attributes have already been
    // retrieved.
    CreateAccelerometerSamplesObserver(id);
  }
}

void AccelerometerProviderMojo::OnAccelerometerRemoteDisconnect(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR)
      << "OnAccelerometerRemoteDisconnect: " << id
      << ", resetting SensorService as IIO Service should be destructed and "
         "waiting for the relaunch of it.";
  ResetSensorService();
}

void AccelerometerProviderMojo::GetAttributesCallback(
    int32_t id,
    const std::vector<base::Optional<std::string>>& values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& accelerometer = accelerometers_[id];
  DCHECK(accelerometer.remote.is_bound());

  size_t index = 0;
  if (!accelerometer.location.has_value()) {
    if (index >= values.size()) {
      LOG(ERROR) << "values doesn't contain location attribute.";
      IgnoreAccelerometer(id);
      return;
    }

    if (!values[index].has_value()) {
      LOG(WARNING) << "No location attribute for accel with id: " << id;
      IgnoreAccelerometer(id);
      return;
    }

    auto* it = base::ranges::find(kLocationStrings, values[index]);
    if (it == std::end(kLocationStrings)) {
      LOG(WARNING) << "Unrecognized location: " << values[index].value()
                   << " for device with id: ";
      IgnoreAccelerometer(id);
      return;
    }

    AccelerometerSource source = static_cast<AccelerometerSource>(
        std::distance(std::begin(kLocationStrings), it));
    accelerometer.location = source;

    if (location_to_accelerometer_id_.find(source) !=
        location_to_accelerometer_id_.end()) {
      LOG(ERROR) << "Duplicated location source " << source
                 << " of accel id: " << id
                 << ", and accel id: " << location_to_accelerometer_id_[source];
      FailedToInitialize();
      return;
    }

    location_to_accelerometer_id_[source] = id;
    ++index;
  }

  if (!accelerometer.scale.has_value()) {
    if (index >= values.size()) {
      LOG(ERROR) << "values doesn't contain scale attribute.";
      IgnoreAccelerometer(id);
      return;
    }

    double scale = 0.0;
    if (!values[index].has_value() ||
        !base::StringToDouble(values[index].value(), &scale)) {
      LOG(ERROR) << "Invalid scale: " << values[index].value_or("")
                 << ", for accel with id: " << id;
      IgnoreAccelerometer(id);
      return;
    }
    accelerometer.scale = scale;

    ++index;
  }

  CheckInitialization();

  CreateAccelerometerSamplesObserver(id);
}

void AccelerometerProviderMojo::IgnoreAccelerometer(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& accelerometer = accelerometers_[id];

  LOG(WARNING) << "Ignoring accel with id: " << id;
  accelerometer.ignored = true;
  accelerometer.remote.reset();

  CheckInitialization();
}

void AccelerometerProviderMojo::CheckInitialization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(GetECLidAngleDriverStatus(), ECLidAngleDriverStatus::UNKNOWN);

  if (initialization_state_ != State::INITIALIZING)
    return;

  bool has_accelerometer_lid = false;
  for (const auto& accelerometer : accelerometers_) {
    if (accelerometer.second.ignored) {
      if (!accelerometer.second.location.has_value())
        continue;

      if (accelerometer.second.location == ACCELEROMETER_SOURCE_SCREEN ||
          GetECLidAngleDriverStatus() ==
              ECLidAngleDriverStatus::NOT_SUPPORTED) {
        // This ignored accelerometer is essential.
        FailedToInitialize();
        return;
      }

      continue;
    }

    if (!accelerometer.second.scale.has_value() ||
        !accelerometer.second.location.has_value())
      return;

    if (accelerometer.second.location == ACCELEROMETER_SOURCE_SCREEN)
      has_accelerometer_lid = true;
    else
      has_accelerometer_base_ = true;
  }

  if (has_accelerometer_lid) {
    if (!has_accelerometer_base_) {
      LOG(WARNING)
          << "Initialization succeeded without an accelerometer on the base";
    }

    initialization_state_ = State::SUCCESS;
  } else {
    FailedToInitialize();
  }
}

void AccelerometerProviderMojo::CreateAccelerometerSamplesObserver(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& accelerometer = accelerometers_[id];
  DCHECK(accelerometer.remote.is_bound());
  DCHECK(!accelerometer.ignored);
  DCHECK(accelerometer.scale.has_value() && accelerometer.location.has_value());

  if (accelerometer.location == ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD &&
      GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::SUPPORTED) {
    // Skipping as it's only needed if lid-angle is not supported.
    // |GetLidAngleIdsCallback| will call this function with this |id| again if
    // it's found not supported.
    return;
  }

  accelerometer.samples_observer =
      std::make_unique<AccelerometerSamplesObserver>(
          id, std::move(accelerometer.remote), accelerometer.scale.value(),
          base::BindRepeating(
              &AccelerometerProviderMojo::OnSampleUpdatedCallback, this));

  if (accelerometer_read_on_)
    accelerometer.samples_observer->SetEnabled(true);
}

void AccelerometerProviderMojo::EnableAccelerometerReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(GetECLidAngleDriverStatus(), ECLidAngleDriverStatus::UNKNOWN);
  if (accelerometer_read_on_)
    return;

  accelerometer_read_on_ = true;
  for (auto& accelerometer : accelerometers_) {
    if (!accelerometer.second.samples_observer.get())
      continue;

    accelerometer.second.samples_observer->SetEnabled(true);
  }
}

void AccelerometerProviderMojo::DisableAccelerometerReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetECLidAngleDriverStatus(), ECLidAngleDriverStatus::SUPPORTED);
  if (!accelerometer_read_on_)
    return;

  accelerometer_read_on_ = false;

  for (auto& accelerometer : accelerometers_) {
    if (!accelerometer.second.samples_observer.get())
      continue;

    accelerometer.second.samples_observer->SetEnabled(false);
  }
}

void AccelerometerProviderMojo::OnSampleUpdatedCallback(
    int iio_device_id,
    std::vector<float> sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(sample.size(), kNumberOfAxes);

  auto& accelerometer = accelerometers_[iio_device_id];
  DCHECK(accelerometer.location.has_value());

  bool need_two_accelerometers =
      (GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::NOT_SUPPORTED &&
       has_accelerometer_base_);

  if (!accelerometer_read_on_) {
    // This sample is not needed.
    return;
  }

  update_.Set(accelerometers_[iio_device_id].location.value(), sample[0],
              sample[1], sample[2]);

  if (need_two_accelerometers &&
      (!update_.has(ACCELEROMETER_SOURCE_SCREEN) ||
       !update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD))) {
    // Wait for the other accel to be updated.
    return;
  }

  NotifyAccelerometerUpdated(update_);
  update_.Reset();
}

void AccelerometerProviderMojo::FailedToInitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(initialization_state_, State::SUCCESS);

  LOG(ERROR) << "Failed to initialize for accelerometer read.";
  initialization_state_ = State::FAILED;

  accelerometers_.clear();
  ResetSensorService();
  sensor_hal_client_.reset();
}

}  // namespace ash
