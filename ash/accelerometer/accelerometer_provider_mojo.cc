// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_provider_mojo.h"

#include <iterator>
#include <utility>

#include "ash/accelerometer/accelerometer_constants.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"

namespace ash {

namespace {

// Delay of the reconnection to Sensor Hal Dispatcher.
constexpr base::TimeDelta kDelayReconnect = base::Milliseconds(1000);

// Timeout for the late-present devices: 10 seconds.
constexpr base::TimeDelta kNewDevicesTimeout = base::Milliseconds(10000);

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
  SetNewDevicesObserver();

  QueryDevices();
}

void AccelerometerProviderMojo::OnNewDeviceAdded(
    int32_t iio_device_id,
    const std::vector<chromeos::sensors::mojom::DeviceType>& types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(initialization_state_, MojoState::ANGL_LID);

  for (const auto& type : types) {
    if (type == chromeos::sensors::mojom::DeviceType::ACCEL) {
      if (initialization_state_ == MojoState::LID_BASE) {
        // Don't need a new accelerometer.
        continue;
      }

      if (base::Contains(accelerometers_, iio_device_id))
        continue;

      RegisterAccelerometerWithId(iio_device_id);
    } else if (type == chromeos::sensors::mojom::DeviceType::ANGL) {
      SetECLidAngleDriverSupported();
    }
  }
}

MojoState AccelerometerProviderMojo::GetInitializationStateForTesting() const {
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

  auto* dispatcher = chromeos::sensors::SensorHalDispatcher::GetInstance();
  if (!dispatcher) {
    // In unit tests, SensorHalDispatcher is not initialized.
    return;
  }

  dispatcher->RegisterClient(sensor_hal_client_.BindNewPipeAndPassRemote());

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

  new_devices_observer_.reset();
  sensor_service_remote_.reset();
}

void AccelerometerProviderMojo::ResetStates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  initialization_state_ = MojoState::INITIALIZING;
  accelerometers_.clear();
  location_to_accelerometer_id_.clear();
  update_.Reset();

  if (sensor_service_remote_.is_bound())
    QueryDevices();
}

void AccelerometerProviderMojo::QueryDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_service_remote_.is_bound());

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

void AccelerometerProviderMojo::SetECLidAngleDriverSupported() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::SUPPORTED)
    return;

  DCHECK_NE(initialization_state_, MojoState::ANGL);
  DCHECK_NE(initialization_state_, MojoState::ANGL_LID);

  if (GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::NOT_SUPPORTED) {
    // |GetECLidAngleDriverStatus()| will be set to NOT_SUPPORTED when waiting
    // for new devices is timed out. However, this function may still be called
    // after the timeout and when that happens, we'll need to overwrite the
    // status and revert some changes.
    LOG(WARNING) << "Overwriting ECLidAngleDriverStatus from NOT_SUPPORTED "
                    "to SUPPORTED";
  }

  SetECLidAngleDriverStatus(ECLidAngleDriverStatus::SUPPORTED);

  if (pending_on_tablet_physical_state_changed_)
    OnTabletPhysicalStateChanged();

  UpdateStateWithECLidAngleDriverSupported();
}

void AccelerometerProviderMojo::UpdateStateWithECLidAngleDriverSupported() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (initialization_state_) {
    case MojoState::INITIALIZING:
      initialization_state_ = MojoState::ANGL;
      break;

    case MojoState::BASE: {
      initialization_state_ = MojoState::ANGL;

      // Ignores the base-accelerometer as it's no longer needed for the only
      // use case: calculating the angle between the lid and the base, which is
      // substituted by the driver.
      auto it = location_to_accelerometer_id_.find(
          ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
      DCHECK(it != location_to_accelerometer_id_.end());
      IgnoreAccelerometer(it->second);
      break;
    }

    case MojoState::LID:
      initialization_state_ = MojoState::ANGL_LID;
      break;

    case MojoState::LID_BASE: {
      initialization_state_ = MojoState::ANGL_LID;

      // Ignores the base-accelerometer as it's no longer needed for the only
      // use case: calculating the angle between the lid and the base, which is
      // substituted by the driver.
      auto it = location_to_accelerometer_id_.find(
          ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
      DCHECK(it != location_to_accelerometer_id_.end());
      IgnoreAccelerometer(it->second);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected state: "
                 << static_cast<int32_t>(initialization_state_);
  }

  if (initialization_state_ == MojoState::ANGL_LID)
    new_devices_observer_.reset();
}

void AccelerometerProviderMojo::UpdateStateWithLidAccelerometer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (initialization_state_) {
    case MojoState::INITIALIZING:
      initialization_state_ = MojoState::LID;
      break;

    case MojoState::BASE: {
      initialization_state_ = MojoState::LID_BASE;

      auto it = location_to_accelerometer_id_.find(
          ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
      DCHECK(it != location_to_accelerometer_id_.end());
      if (accelerometers_[it->second].samples_observer.get())
        accelerometers_[it->second].samples_observer->SetEnabled(true);

      break;
    }

    case MojoState::ANGL:
      initialization_state_ = MojoState::ANGL_LID;
      new_devices_observer_.reset();
      break;

    default:
      LOG(FATAL) << "Unexpected state: "
                 << static_cast<int32_t>(initialization_state_);
  }
}

void AccelerometerProviderMojo::UpdateStateWithBaseAccelerometer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (initialization_state_) {
    case MojoState::INITIALIZING:
      initialization_state_ = MojoState::BASE;
      break;

    case MojoState::LID:
      initialization_state_ = MojoState::LID_BASE;
      break;

    case MojoState::ANGL:
    case MojoState::ANGL_LID: {
      // Ignores the base-accelerometer as it's no longer needed for the only
      // use case: calculating the angle between the lid and the base, which is
      // substituted by the driver.
      auto it = location_to_accelerometer_id_.find(
          ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
      DCHECK(it != location_to_accelerometer_id_.end());
      IgnoreAccelerometer(it->second);
      break;
    }

    default:
      LOG(FATAL) << "Unexpected state: "
                 << static_cast<int32_t>(initialization_state_);
  }
}

void AccelerometerProviderMojo::SetNewDevicesObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_service_remote_.is_bound());
  DCHECK(!new_devices_observer_.is_bound());

  if (initialization_state_ == MojoState::ANGL_LID) {
    // Don't need any further devices.
    return;
  }

  sensor_service_remote_->RegisterNewDevicesObserver(
      new_devices_observer_.BindNewPipeAndPassRemote());
  new_devices_observer_.set_disconnect_handler(base::BindOnce(
      &AccelerometerProviderMojo::OnNewDevicesObserverDisconnect, this));

  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AccelerometerProviderMojo::OnNewDevicesTimeout, this),
      kNewDevicesTimeout);
}

void AccelerometerProviderMojo::AccelerometerProviderMojo::
    OnNewDevicesObserverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR)
      << "OnNewDevicesObserverDisconnect, resetting SensorService as IIO "
         "Service should be destructed and waiting for the relaunch of it.";
  ResetSensorService();
}

void AccelerometerProviderMojo::OnNewDevicesTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sensor_service_remote_.is_bound()) {
    // Skips and waits for the next timeout for the case that IIO Service
    // disconnects after the first connection.
    return;
  }

  if (GetECLidAngleDriverStatus() != ECLidAngleDriverStatus::UNKNOWN)
    return;

  if (initialization_state_ == MojoState::INITIALIZING ||
      initialization_state_ == MojoState::BASE ||
      initialization_state_ == MojoState::ANGL) {
    LOG(ERROR) << "Unfinished initialization after timeout: "
               << static_cast<int32_t>(initialization_state_);
  }

  SetECLidAngleDriverStatus(ECLidAngleDriverStatus::NOT_SUPPORTED);
  EnableAccelerometerReading();

  if (pending_on_tablet_physical_state_changed_)
    OnTabletPhysicalStateChanged();
}

void AccelerometerProviderMojo::GetLidAngleIdsCallback(
    const std::vector<int32_t>& lid_angle_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!lid_angle_ids.empty())
    SetECLidAngleDriverSupported();
}

void AccelerometerProviderMojo::GetAccelerometerIdsCallback(
    const std::vector<int32_t>& accelerometer_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

  if (accelerometer.remote.is_bound()) {
    // Has already been registered.
    return;
  }
  DCHECK(!accelerometer.samples_observer.get());

  if (!sensor_service_remote_.is_bound()) {
    // Something went wrong. Skipping here.
    return;
  }

  sensor_service_remote_->GetDevice(
      id, accelerometer.remote.BindNewPipeAndPassReceiver());
  accelerometer.remote.set_disconnect_with_reason_handler(base::BindOnce(
      &AccelerometerProviderMojo::OnAccelerometerRemoteDisconnect, this, id));

  std::vector<std::string> attr_names;
  if (!accelerometer.location.has_value())
    attr_names.push_back(chromeos::sensors::mojom::kLocation);
  if (!accelerometer.scale.has_value())
    attr_names.push_back(chromeos::sensors::mojom::kScale);

  if (attr_names.empty()) {
    // Create the observer directly if the attributes have already been
    // retrieved.
    CreateAccelerometerSamplesObserver(id);

    return;
  }

  if (initialization_state_ == MojoState::ANGL_LID ||
      initialization_state_ == MojoState::LID_BASE) {
    // No need of new accelerometers.
    return;
  }

  accelerometer.remote->GetAttributes(
      attr_names,
      base::BindOnce(&AccelerometerProviderMojo::GetAttributesCallback, this,
                     id));
}

void AccelerometerProviderMojo::OnAccelerometerRemoteDisconnect(
    int32_t id,
    uint32_t custom_reason_code,
    const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& accelerometer = accelerometers_[id];
  auto reason =
      static_cast<chromeos::sensors::mojom::SensorDeviceDisconnectReason>(
          custom_reason_code);
  LOG(WARNING) << "OnAccelerometerRemoteDisconnect: " << id
               << ", reason: " << reason << ", description: " << description;

  switch (reason) {
    case chromeos::sensors::mojom::SensorDeviceDisconnectReason::
        IIOSERVICE_CRASHED:
      ResetSensorService();
      break;

    case chromeos::sensors::mojom::SensorDeviceDisconnectReason::DEVICE_REMOVED:
      // This accelerometer is not in use.
      if (accelerometer.ignored || !accelerometer.location.has_value() ||
          !accelerometer.scale.has_value()) {
        accelerometers_.erase(id);
      } else {
        // Reset usages & states, and restart the mojo devices initialization.
        ResetStates();
      }
      break;
  }
}

void AccelerometerProviderMojo::GetAttributesCallback(
    int32_t id,
    const std::vector<std::optional<std::string>>& values) {
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

    if (base::Contains(location_to_accelerometer_id_, source)) {
      LOG(WARNING) << "Duplicated location source " << source
                   << " of accel id: " << id << ", and accel id: "
                   << location_to_accelerometer_id_[source];
      IgnoreAccelerometer(id);
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

  if (accelerometer.location == ACCELEROMETER_SOURCE_SCREEN) {
    UpdateStateWithLidAccelerometer();
  } else {
    DCHECK_EQ(accelerometer.location.value(),
              ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
    UpdateStateWithBaseAccelerometer();
  }

  if (accelerometer.ignored) {
    // base-accelerometer is not needed if EC Lid Angle Driver is supported.
    return;
  }

  CreateAccelerometerSamplesObserver(id);
}

void AccelerometerProviderMojo::IgnoreAccelerometer(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& accelerometer = accelerometers_[id];

  LOG(WARNING) << "Ignoring accel with id: " << id;
  accelerometer.ignored = true;
  accelerometer.remote.reset();
  accelerometer.samples_observer.reset();
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

  accelerometer.samples_observer = std::make_unique<AccelGyroSamplesObserver>(
      id, std::move(accelerometer.remote), accelerometer.scale.value(),
      base::BindRepeating(&AccelerometerProviderMojo::OnSampleUpdatedCallback,
                          this));

  if (initialization_state_ == MojoState::BASE) {
    DCHECK_EQ(accelerometer.location.value(),
              ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);
    // Don't need base-accelerometer's samples without lid-accelerometer.
    return;
  }

  if (accelerometer_read_on_)
    accelerometer.samples_observer->SetEnabled(true);
}

void AccelerometerProviderMojo::EnableAccelerometerReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(GetECLidAngleDriverStatus(), ECLidAngleDriverStatus::UNKNOWN);
  if (accelerometer_read_on_)
    return;

  accelerometer_read_on_ = true;

  if (initialization_state_ == MojoState::BASE) {
    // Don't need base-accelerometer's samples without lid-accelerometer.
    return;
  }

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
  DCHECK(accelerometer.location == ACCELEROMETER_SOURCE_SCREEN ||
         initialization_state_ == MojoState::LID_BASE);

  if (!accelerometer_read_on_) {
    // This sample is not needed.
    return;
  }

  update_.Set(accelerometers_[iio_device_id].location.value(), sample[0],
              sample[1], sample[2]);

  if (initialization_state_ == MojoState::LID_BASE &&
      (!update_.has(ACCELEROMETER_SOURCE_SCREEN) ||
       !update_.has(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD))) {
    // Wait for the other accelerometer to be updated.
    return;
  }

  NotifyAccelerometerUpdated(update_);
  update_.Reset();
}

}  // namespace ash
