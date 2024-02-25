// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sensor_info/sensor_provider.h"

#include <cstddef>
#include <iterator>
#include <utility>

#include "ash/sensor_info/sensor_types.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"

namespace ash {

using ::chromeos::sensors::mojom::DeviceType;

namespace {

// Delay of the reconnection to Sensor Hal Dispatcher.
constexpr base::TimeDelta kDelayReconnect = base::Seconds(1);

// Timeout for getting lid_angle samples.
constexpr base::TimeDelta kLidAngleTimeout = base::Seconds(1);

constexpr double kReadFrequencyInHz = 100.0;

}  // namespace

SensorProvider::DeviceState::DeviceState() {
  for (int index = 0; index < static_cast<int>(SensorType::kSensorTypeCount);
       ++index) {
    present_.push_back(false);
  }
}

SensorProvider::DeviceState::~DeviceState() = default;

bool SensorProvider::DeviceState::AllSensorsFound() const {
  for (int index = 0; index < static_cast<int>(SensorType::kSensorTypeCount);
       ++index) {
    if (!present_[index]) {
      return false;
    }
  }
  return true;
}

void SensorProvider::DeviceState::Reset() {
  for (int index = 0; index < static_cast<int>(SensorType::kSensorTypeCount);
       ++index) {
    present_[index] = false;
  }
}

bool SensorProvider::DeviceState::CompareUpdate(SensorUpdate update) const {
  for (int index = 0; index < static_cast<int>(SensorType::kSensorTypeCount);
       ++index) {
    auto source = static_cast<SensorType>(index);
    if (!present_[index]) {
      if (update.has(source)) {
        LOG(ERROR) << "SensorUpdate has extra source: "
                   << static_cast<int>(source);
      }
      continue;
    }
    if (update.has(source)) {
      continue;
    }
    return false;
  }
  return true;
}

void SensorProvider::DeviceState::AddSource(SensorType source) {
  present_[static_cast<int>(source)] = true;
}

void SensorProvider::DeviceState::RemoveSource(SensorType source) {
  present_[static_cast<int>(source)] = false;
}

bool SensorProvider::DeviceState::GetSource(SensorType source) const {
  return present_[static_cast<int>(source)];
}

std::vector<bool> SensorProvider::DeviceState::GetStatesForTesting() const {
  return present_;
}

SensorProvider::SensorData::SensorData() = default;
SensorProvider::SensorData::~SensorData() = default;

SensorProvider::SensorProvider() {
  RegisterSensorClient();
}

SensorProvider::~SensorProvider() = default;

void SensorProvider::SetUpChannel(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
        pending_remote) {
  if (sensor_service_remote_.is_bound()) {
    LOG(ERROR) << "Ignoring the second Remote<SensorService>";
    return;
  }

  sensor_service_remote_.Bind(std::move(pending_remote));
  sensor_service_remote_.set_disconnect_handler(
      base::BindOnce(&SensorProvider::OnSensorServiceDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  SetNewDevicesObserver();

  QueryDevices();
}

void SensorProvider::OnSensorServiceDisconnect() {
  LOG(ERROR) << "OnSensorServiceDisconnect";

  ResetSensorService();
}

std::vector<bool> SensorProvider::GetStateForTesting() {
  return current_state_.GetStatesForTesting();  // IN-TEST
}

void SensorProvider::OnNewDeviceAdded(int32_t iio_device_id,
                                      const std::vector<DeviceType>& types) {
  for (const auto& type : types) {
    if (type == DeviceType::ACCEL || type == DeviceType::ANGL ||
        type == DeviceType::ANGLVEL) {
      RegisterSensor(type, iio_device_id);
    }
  }
}

void SensorProvider::RegisterSensorClient() {
  auto* dispatcher = chromeos::sensors::SensorHalDispatcher::GetInstance();
  if (!dispatcher) {
    // In unit tests, SensorHalDispatcher is not initialized.
    return;
  }

  dispatcher->RegisterClient(sensor_hal_client_.BindNewPipeAndPassRemote());

  sensor_hal_client_.set_disconnect_handler(
      base::BindOnce(&SensorProvider::OnSensorHalClientFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SensorProvider::OnSensorHalClientFailure() {
  LOG(ERROR) << "OnSensorHalClientFailure";

  ResetSensorService();
  sensor_hal_client_.reset();
  // It's expected that SensorHalDispatcher will restart after failure,
  // so it's OK to retry forever here.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SensorProvider::RegisterSensorClient,
                     weak_ptr_factory_.GetWeakPtr()),
      kDelayReconnect);
}

void SensorProvider::SetNewDevicesObserver() {
  DCHECK(sensor_service_remote_.is_bound());
  DCHECK(!new_devices_observer_.is_bound());
  if (current_state_.AllSensorsFound()) {
    // Don't need any further devices.
    return;
  }
  sensor_service_remote_->RegisterNewDevicesObserver(
      new_devices_observer_.BindNewPipeAndPassRemote());
  new_devices_observer_.set_disconnect_handler(
      base::BindOnce(&SensorProvider::OnNewDevicesObserverDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SensorProvider::OnNewDevicesObserverDisconnect() {
  LOG(ERROR)
      << "OnNewDevicesObserverDisconnect, resetting SensorService as IIO "
         "Service should be destructed and waiting for the relaunch of it.";
  ResetSensorService();
}

void SensorProvider::ResetSensorService() {
  new_devices_observer_.reset();
  sensor_service_remote_.reset();
  // Discard SensorData to prevent sensors removed while disconnected.
  ResetStates();
}

void SensorProvider::QueryDevices() {
  DCHECK(sensor_service_remote_.is_bound());
  // Get lid_angle sensors
  sensor_service_remote_->GetDeviceIds(
      DeviceType::ANGL, base::BindOnce(&SensorProvider::OnLidAngleIds,
                                       weak_ptr_factory_.GetWeakPtr()));
  // Get accelerometer sensors
  sensor_service_remote_->GetDeviceIds(
      DeviceType::ACCEL, base::BindOnce(&SensorProvider::OnAccelerometerIds,
                                        weak_ptr_factory_.GetWeakPtr()));
  // Get gyroscope sensors
  sensor_service_remote_->GetDeviceIds(
      DeviceType::ANGLVEL, base::BindOnce(&SensorProvider::OnGyroscopeIds,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void SensorProvider::OnLidAngleIds(const std::vector<int32_t>& lid_angle_ids) {
  for (int32_t id : lid_angle_ids) {
    RegisterSensor(DeviceType::ANGL, id);
  }
}

void SensorProvider::OnAccelerometerIds(
    const std::vector<int32_t>& accelerometer_ids) {
  for (int32_t id : accelerometer_ids) {
    RegisterSensor(DeviceType::ACCEL, id);
  }
}

void SensorProvider::OnGyroscopeIds(const std::vector<int32_t>& gyroscope_ids) {
  for (int32_t id : gyroscope_ids) {
    RegisterSensor(DeviceType::ANGLVEL, id);
  }
}

void SensorProvider::RegisterSensor(DeviceType device_type, int32_t id) {
  auto& sensor = sensors_[id][device_type];

  if (sensor.ignored) {
    // Something went wrong in the previous initialization. Ignoring this
    // sensor.
    return;
  }

  if (sensor.remote.is_bound()) {
    // Has already been registered.
    return;
  }
  DCHECK(!sensor.samples_observer.get())
      << "Registering a sensor for device id " << id << " type " << device_type
      << "when there already exists one.";
  if (!sensor_service_remote_.is_bound()) {
    // Something went wrong. Skipping here.
    LOG(ERROR) << "SensorService is not initialized.";
    return;
  }

  sensor_service_remote_->GetDevice(id,
                                    sensor.remote.BindNewPipeAndPassReceiver());
  sensor.remote.set_disconnect_with_reason_handler(
      base::BindOnce(&SensorProvider::OnSensorRemoteDisconnect,
                     weak_ptr_factory_.GetWeakPtr(), device_type, id));
  if (device_type == DeviceType::ANGL) {
    // Samples from ANGL range from 0-180 (which means lid_angle degree), so set
    // scale to 1.
    // Location of lid_angle device is meaningless, so set to kOther.
    sensor.location = SensorLocation::kOther;
    sensor.scale = 1.0;
    if (base::Contains(type_to_sensor_id_, SensorType::kLidAngle)) {
      LOG(WARNING) << "Duplicated location source "
                   << "id_angle"
                   << " of sensor id: " << id << ", and sensor id: "
                   << type_to_sensor_id_[SensorType::kLidAngle];
      IgnoreSensor(device_type, id);
      return;
    }
    current_state_.AddSource(SensorType::kLidAngle);
    type_to_sensor_id_[SensorType::kLidAngle] = id;
  }
  std::vector<std::string> attr_names;
  if (!sensor.location.has_value()) {
    attr_names.push_back(chromeos::sensors::mojom::kLocation);
  }
  if (!sensor.scale.has_value()) {
    attr_names.push_back(chromeos::sensors::mojom::kScale);
  }
  if (attr_names.empty() || device_type == DeviceType::ANGL) {
    // Create the observer directly if the attributes have already been
    // retrieved. Won't create SamplesObserver for lid_angle sensor.
    CreateSensorSamplesObserver(device_type, id);

    return;
  }

  sensor.remote->GetAttributes(
      attr_names,
      base::BindOnce(&SensorProvider::OnAttributes,
                     weak_ptr_factory_.GetWeakPtr(), device_type, id));
}

void SensorProvider::OnSensorRemoteDisconnect(DeviceType device_type,
                                              int32_t id,
                                              uint32_t custom_reason_code,
                                              const std::string& description) {
  auto& sensor = sensors_[id][device_type];
  auto reason =
      static_cast<chromeos::sensors::mojom::SensorDeviceDisconnectReason>(
          custom_reason_code);
  LOG(WARNING) << "OnSensorRemoteDisconnect: " << id << ", reason: " << reason
               << ", description: " << description;

  switch (reason) {
    case chromeos::sensors::mojom::SensorDeviceDisconnectReason::
        IIOSERVICE_CRASHED:
      ResetSensorService();
      break;

    case chromeos::sensors::mojom::SensorDeviceDisconnectReason::DEVICE_REMOVED:
      // This sensor is not in use.
      if (sensor.ignored || !sensor.location.has_value() ||
          !sensor.scale.has_value()) {
        sensors_[id].erase(device_type);
      } else {
        // Reset usages & states, and restart the mojo devices initialization.
        ResetStates();
      }
      break;
  }
}

void SensorProvider::ResetStates() {
  current_state_.Reset();
  sensors_.clear();
  type_to_sensor_id_.clear();
  update_.Reset();
  if (sensor_service_remote_.is_bound()) {
    QueryDevices();
  }
}

void SensorProvider::IgnoreSensor(DeviceType device_type, int32_t id) {
  auto& sensor = sensors_[id][device_type];

  LOG(WARNING) << "Ignoring sensor with id: " << id
               << " device_type:" << device_type;

  sensor.ignored = true;
  sensor.remote.reset();
  sensor.samples_observer.reset();
}

void SensorProvider::EnableSensorReading() {
  sensor_read_on_ = true;
  EnableSensorReadingInternal();
}

void SensorProvider::EnableSensorReadingInternal() {
  // Starts Sensor Reading if all sensors are ready.
  if (sensor_read_on_ && CheckSensorSamplesObserver()) {
    GetLidAngleUpdate();
    for (auto& sensor : sensors_) {
      for (auto& type : sensor.second) {
        if (!type.second.samples_observer.get()) {
          continue;
        }
        type.second.samples_observer->SetEnabled(true);
      }
    }
  }
}

void SensorProvider::StopSensorReading() {
  for (auto& sensor : sensors_) {
    for (auto& type : sensor.second) {
      if (!type.second.samples_observer.get()) {
        continue;
      }
      type.second.samples_observer->SetEnabled(false);
    }
  }
  sensor_read_on_ = false;
}

void SensorProvider::CreateSensorSamplesObserver(DeviceType device_type,
                                                 int32_t id) {
  auto& sensor = sensors_[id][device_type];
  DCHECK(sensor.remote.is_bound());
  DCHECK(!sensor.ignored);
  DCHECK(sensor.scale.has_value() && sensor.location.has_value());
  if (device_type == DeviceType::ACCEL || device_type == DeviceType::ANGLVEL) {
    sensor.samples_observer = std::make_unique<AccelGyroSamplesObserver>(
        id, std::move(sensor.remote), sensor.scale.value(),
        base::BindRepeating(&SensorProvider::OnSampleUpdatedCallback,
                            weak_ptr_factory_.GetWeakPtr(), device_type),
        device_type, kReadFrequencyInHz);
  }
  EnableSensorReadingInternal();
}

bool SensorProvider::CheckSensorSamplesObserver() {
  for (auto& sensor_id : sensors_) {
    for (auto& [type, sensor_data] : sensor_id.second) {
      if (type == DeviceType::ANGL || sensor_data.ignored) {
        continue;
      }
      if (!sensor_data.samples_observer.get()) {
        return false;
      }
    }
  }
  return true;
}

void SensorProvider::OnAttributes(
    DeviceType device_type,
    int32_t id,
    const std::vector<std::optional<std::string>>& values) {
  auto& sensor = sensors_[id][device_type];
  DCHECK(sensor.remote.is_bound());
  auto val_it = values.begin();
  SensorType source;
  if (!sensor.location.has_value()) {
    if (val_it == values.end()) {
      LOG(ERROR) << "values doesn't contain location attribute.";
      IgnoreSensor(device_type, id);
      return;
    }

    if (!val_it->has_value()) {
      LOG(WARNING) << "No location attribute for sensor with id: " << id;
      IgnoreSensor(device_type, id);
      return;
    }

    auto* it = base::ranges::find(kLocationStrings, val_it->value());
    if (it == std::end(kLocationStrings)) {
      LOG(WARNING) << "Unrecognized location: " << val_it->value()
                   << " for device with id: " << id;
      IgnoreSensor(device_type, id);
      return;
    }

    SensorLocation location = static_cast<SensorLocation>(
        std::distance(std::begin(kLocationStrings), it));
    sensor.location = location;
    if (device_type == DeviceType::ACCEL) {
      if (location == SensorLocation::kBase) {
        source = SensorType::kAccelerometerBase;
      } else {
        source = SensorType::kAccelerometerLid;
      }
    } else {
      if (location == SensorLocation::kBase) {
        source = SensorType::kGyroscopeBase;
      } else {
        source = SensorType::kGyroscopeLid;
      }
    }
    if (base::Contains(type_to_sensor_id_, source)) {
      LOG(WARNING) << "Duplicated location source " << static_cast<int>(source)
                   << " of sensor id: " << id
                   << ", and sensor id: " << type_to_sensor_id_[source];
      IgnoreSensor(device_type, id);
      return;
    }
    val_it++;
  }

  if (!sensor.scale.has_value()) {
    if (val_it == values.end()) {
      LOG(ERROR) << "values doesn't contain scale attribute.";
      IgnoreSensor(device_type, id);
      return;
    }

    double scale = 0.0;
    if (!val_it->has_value() ||
        !base::StringToDouble(val_it->value(), &scale)) {
      LOG(ERROR) << "Invalid scale: " << val_it->value_or("")
                 << ", for accel with id: " << id;
      IgnoreSensor(device_type, id);
      return;
    }
    sensor.scale = scale;
  }

  DCHECK(!sensor.ignored);
  current_state_.AddSource(source);
  type_to_sensor_id_[source] = id;
  CreateSensorSamplesObserver(device_type, id);
}

void SensorProvider::GetLidAngleUpdate() {
  if (current_state_.GetSource(SensorType::kLidAngle) &&
      !update_.has(SensorType::kLidAngle)) {
    sensors_[type_to_sensor_id_[SensorType::kLidAngle]][DeviceType::ANGL]
        .remote->GetChannelsAttributes(
            {0}, {"raw"},
            base::BindRepeating(&SensorProvider::OnLidAngleValue,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

void SensorProvider::OnSampleUpdatedCallback(DeviceType device_type,
                                             int iio_device_id,
                                             std::vector<float> sample) {
  DCHECK_EQ(sample.size(), kNumberOfAxes);
  SensorType key;
  bool find = false;
  for (auto& i : type_to_sensor_id_) {
    if (i.second == iio_device_id) {
      key = i.first;
      if ((key == SensorType::kAccelerometerBase ||
           key == SensorType::kAccelerometerLid) &&
          device_type == DeviceType::ACCEL) {
        find = true;
        break;
      } else if ((key == SensorType::kGyroscopeBase ||
                  key == SensorType::kGyroscopeLid) &&
                 device_type == DeviceType::ANGLVEL) {
        find = true;
        break;
      }
    }
  }
  if (!find) {
    LOG(ERROR) << "Couldn't find the the SensorType that matches DeviceType: "
               << device_type << " and device_id: " << iio_device_id;
    return;
  }

  update_.Set(key, sample[0], sample[1], sample[2]);

  if (!current_state_.CompareUpdate(update_)) {
    // Wait for other sensors to be updated.
    return;
  }

  NotifySensorUpdated(update_);
}

void SensorProvider::OnLidAngleValue(
    const std::vector<std::optional<std::string>>& values) {
  if (values[0].has_value()) {
    int angle;
    if (base::StringToInt(values[0].value(), &angle)) {
      update_.Set(SensorType::kLidAngle, angle);
      if (current_state_.CompareUpdate(update_)) {
        NotifySensorUpdated(update_);
      }
      return;
    }

    LOG(ERROR) << "Failed to convert lid angle sample into integer.";
  } else {
    LOG(ERROR) << "Lid Angle device couldn't get angle return.";
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SensorProvider::GetLidAngleUpdate,
                     weak_ptr_factory_.GetWeakPtr()),
      kLidAngleTimeout);
}

void SensorProvider::NotifySensorUpdated(SensorUpdate update) {
  for (auto& observer : observers_) {
    observer.OnSensorUpdated(update);
  }
  update_.Reset();

  GetLidAngleUpdate();
}

void SensorProvider::AddObserver(SensorObserver* observer) {
  observers_.AddObserver(observer);
}

void SensorProvider::RemoveObserver(SensorObserver* observer) {
  observers_.RemoveObserver(observer);
}
}  // namespace ash
