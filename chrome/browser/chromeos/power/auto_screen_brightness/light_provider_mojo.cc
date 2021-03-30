// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/light_provider_mojo.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/components/sensors/ash/sensor_hal_dispatcher.h"

namespace {

// Delay of the reconnection to Sensor Hal Dispatcher.
constexpr base::TimeDelta kDelayReconnect =
    base::TimeDelta::FromMilliseconds(1000);

constexpr base::TimeDelta kNewDevicesTimeout =
    base::TimeDelta::FromMilliseconds(10000);

constexpr char kCrosECLightName[] = "cros-ec-light";
constexpr char kAcpiAlsName[] = "acpi-als";

}  // namespace

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

LightProviderMojo::LightProviderMojo(AlsReader* als_reader,
                                     bool has_several_light_sensors)
    : LightProviderInterface(als_reader),
      has_several_light_sensors_(has_several_light_sensors) {
  RegisterSensorClient();
}

LightProviderMojo::~LightProviderMojo() = default;

void LightProviderMojo::SetUpChannel(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
        pending_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sensor_service_remote_.is_bound()) {
    LOG(ERROR) << "Ignoring the second Remote<SensorService>";
    return;
  }

  DCHECK(!new_devices_observer_.is_bound());

  sensor_service_remote_.Bind(std::move(pending_remote));
  sensor_service_remote_.set_disconnect_handler(
      base::BindOnce(&LightProviderMojo::OnSensorServiceDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));

  if (light_device_id_.has_value()) {
    SetupLightSamplesObserver();

    auto& light = lights_[light_device_id_.value()];
    DCHECK(!light.ignored);
    DCHECK(light.name.has_value());

    if (has_several_light_sensors_) {
      // The used sensor is cros-ec-light on the lid.
      DCHECK(light.name.value().compare(kCrosECLightName) == 0);
      DCHECK(light.on_lid);
      return;
    }

    if (light.name.value().compare(kCrosECLightName) == 0)
      return;
  }

  sensor_service_remote_->RegisterNewDevicesObserver(
      new_devices_observer_.BindNewPipeAndPassRemote());
  new_devices_observer_.set_disconnect_handler(
      base::BindOnce(&LightProviderMojo::OnNewDevicesObserverDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LightProviderMojo::OnNewDevicesTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      kNewDevicesTimeout);

  sensor_service_remote_->GetDeviceIds(
      chromeos::sensors::mojom::DeviceType::LIGHT,
      base::BindOnce(&LightProviderMojo::GetLightIdsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LightProviderMojo::OnNewDeviceAdded(
    int32_t iio_device_id,
    const std::vector<chromeos::sensors::mojom::DeviceType>& types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (std::find(types.begin(), types.end(),
                chromeos::sensors::mojom::DeviceType::LIGHT) == types.end()) {
    // Not a light sensor. Ignoring this device.
    return;
  }

  RegisterLightWithId(iio_device_id);
}

LightProviderMojo::LightData::LightData() = default;
LightProviderMojo::LightData::~LightData() = default;

void LightProviderMojo::OnNewDevicesObserverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR)
      << "OnNewDevicesObserverDisconnect. Mojo connection to IIO "
         "Service is lost. Resetting the SensorService Mojo channel as well";

  // Assumes IIO Service has crashed and waits for its relaunch.
  ResetSensorService();
}

void LightProviderMojo::OnNewDevicesTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  new_devices_observer_.reset();

  if (light_device_id_.has_value())
    return;

  LOG(ERROR) << "Target light sensor isn't available after timeout. "
                "Initialization failed.";

  lights_.clear();
  ResetSensorService();
  sensor_hal_client_.reset();

  als_reader_->SetAlsInitStatus(AlsReader::AlsInitStatus::kIncorrectConfig);
}

void LightProviderMojo::RegisterSensorClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  chromeos::sensors::SensorHalDispatcher::GetInstance()->RegisterClient(
      sensor_hal_client_.BindNewPipeAndPassRemote());

  sensor_hal_client_.set_disconnect_handler(
      base::BindOnce(&LightProviderMojo::OnSensorHalClientFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LightProviderMojo::OnSensorHalClientFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "OnSensorHalClientFailure. Mojo connection to "
                "SensorHalDispatcher is lost.";

  ResetSensorService();
  sensor_hal_client_.reset();

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LightProviderMojo::RegisterSensorClient,
                     weak_ptr_factory_.GetWeakPtr()),
      kDelayReconnect);
}

void LightProviderMojo::OnSensorServiceDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR)
      << "OnSensorServiceDisconnect. Mojo connection to IIO Service is lost";

  ResetSensorService();
}

void LightProviderMojo::ResetSensorService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_.reset();
  for (auto& light : lights_)
    light.second.remote.reset();

  new_devices_observer_.reset();
  sensor_service_remote_.reset();
}

void LightProviderMojo::GetLightIdsCallback(
    const std::vector<int32_t>& light_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (int32_t id : light_ids)
    RegisterLightWithId(id);
}

void LightProviderMojo::RegisterLightWithId(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_service_remote_.is_bound());

  auto& light = lights_[id];

  if (light.ignored || light.name.has_value() || light.on_lid.has_value()) {
    // The attributes have already been retrieved. Would've been used if chosen
    // in |SetUpChannel|.
    return;
  }

  DCHECK(!light.remote.is_bound());

  light.remote = GetSensorDeviceRemote(id);

  if (has_several_light_sensors_) {
    light.remote->GetAttributes(
        std::vector<std::string>{chromeos::sensors::mojom::kDeviceName,
                                 chromeos::sensors::mojom::kLocation},
        base::BindOnce(&LightProviderMojo::GetNameLocationCallback,
                       weak_ptr_factory_.GetWeakPtr(), id));
  } else {
    light.remote->GetAttributes(
        std::vector<std::string>{chromeos::sensors::mojom::kDeviceName},
        base::BindOnce(&LightProviderMojo::GetNameCallback,
                       weak_ptr_factory_.GetWeakPtr(), id));
  }
}

void LightProviderMojo::GetNameLocationCallback(
    int32_t id,
    const std::vector<base::Optional<std::string>>& values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(light_device_id_.value_or(-1), id);

  if (light_device_id_.has_value()) {
    // Already has the cros-ec-light on the lid. Ignoring other light sensors.
    IgnoreLight(id);
    return;
  }

  if (values.size() < 2) {
    LOG(ERROR) << "Sensor values don't contain name and location attribute";
    IgnoreLight(id);
    return;
  }

  if (values.size() != 2) {
    LOG(WARNING)
        << "Sensor values contain more than name and location attribute. Size: "
        << values.size();
  }

  auto& light = lights_[id];
  DCHECK(light.remote.is_bound());

  light.name = values[0];
  if (!light.name.has_value() ||
      light.name.value().compare(kCrosECLightName) != 0) {
    LOG(ERROR) << "Not " << kCrosECLightName
               << ", sensor name: " << light.name.value_or("");
    IgnoreLight(id);
    return;
  }

  light.on_lid =
      values[1].has_value() &&
      (values[1].value().compare(chromeos::sensors::mojom::kLocationLid) == 0);

  if (!light.on_lid.value()) {
    IgnoreLight(id);
    return;
  }

  DetermineLightSensor(id);
  new_devices_observer_.reset();  // Don't need new light sensors anymore.
}

void LightProviderMojo::GetNameCallback(
    int32_t id,
    const std::vector<base::Optional<std::string>>& values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(light_device_id_.value_or(-1), id);

  if (light_device_id_.has_value()) {
    auto& orig_light = lights_[light_device_id_.value()];
    if (orig_light.name.has_value() &&
        orig_light.name.value().compare(kCrosECLightName) == 0) {
      // Already has the cros-ec-light. Ignoring other light sensors.
      IgnoreLight(id);
      return;
    }
  }

  if (values.empty()) {
    LOG(ERROR) << "Sensor values doesn't contain the name attribute";
    IgnoreLight(id);
    return;
  }

  if (values.size() != 1) {
    LOG(WARNING) << "Sensor values contain more than the name attribute. Size: "
                 << values.size();
  }

  auto& light = lights_[id];
  DCHECK(light.remote.is_bound());

  light.name = values[0];
  if (light.name.has_value() &&
      light.name.value().compare(kCrosECLightName) == 0) {
    // If an acpi-als was chosen, migrate to this cros-ec-light light sensor.
    DetermineLightSensor(id);
    new_devices_observer_.reset();  // Don't need new light sensors anymore.
    return;
  }

  if (!light.name.has_value() ||
      light.name.value().compare(kAcpiAlsName) != 0) {
    LOG(WARNING) << "Unexpected light name: " << light.name.value_or("");
  }

  if (light_device_id_.has_value()) {
    LOG(WARNING) << "Already have another light sensor with name: "
                 << lights_[light_device_id_.value()].name.value_or("");
    IgnoreLight(id);
    return;
  }

  DetermineLightSensor(id);
}

void LightProviderMojo::IgnoreLight(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& light = lights_[id];
  light.ignored = true;
  light.remote.reset();
}

mojo::Remote<chromeos::sensors::mojom::SensorDevice>
LightProviderMojo::GetSensorDeviceRemote(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sensor_service_remote_.is_bound());

  auto& light = lights_[id];
  if (light.remote.is_bound()) {
    // Reuse the previous remote.
    return std::move(light.remote);
  }

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote;
  sensor_service_remote_->GetDevice(
      id, sensor_device_remote.BindNewPipeAndPassReceiver());
  sensor_device_remote.set_disconnect_handler(
      base::BindOnce(&LightProviderMojo::OnLightRemoteDisconnect,
                     weak_ptr_factory_.GetWeakPtr(), id));

  return sensor_device_remote;
}

void LightProviderMojo::OnLightRemoteDisconnect(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << "OnLightRemoteDisconnect: " << id
             << ". Mojo connection to IIO Service is lost. Resetting the "
                "SensorService Mojo channel as well";

  // Assumes IIO Service has crashed and waits for its relaunch.
  ResetSensorService();
}

void LightProviderMojo::DetermineLightSensor(int32_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!light_device_id_.has_value())
    als_reader_->SetAlsInitStatus(AlsReader::AlsInitStatus::kSuccess);

  light_device_id_ = id;
  SetupLightSamplesObserver();
}

void LightProviderMojo::SetupLightSamplesObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(light_device_id_.has_value());

  observer_ = std::make_unique<LightSamplesObserver>(
      als_reader_, GetSensorDeviceRemote(light_device_id_.value()));
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
