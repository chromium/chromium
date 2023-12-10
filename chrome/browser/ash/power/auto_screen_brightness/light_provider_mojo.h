// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_LIGHT_PROVIDER_MOJO_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_LIGHT_PROVIDER_MOJO_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"
#include "chrome/browser/ash/power/auto_screen_brightness/light_samples_observer.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

class LightProviderMojoTest;

// Used when IIO Service is present. It registers to Sensor Hal Dispatcher as a
// sensor client and waits for the connection to CrOS IIO Service. Once
// connected, it asks for samples of the light sensor (cros-ec-light or
// acpi-als).
class LightProviderMojo
    : public LightProviderInterface,
      public chromeos::sensors::mojom::SensorHalClient,
      public chromeos::sensors::mojom::SensorServiceNewDevicesObserver {
 public:
  explicit LightProviderMojo(AlsReader* als_reader);
  LightProviderMojo(const LightProviderMojo&) = delete;
  LightProviderMojo& operator=(const LightProviderMojo&) = delete;
  ~LightProviderMojo() override;

  // chromeos::sensors::mojom::SensorHalClient:
  void SetUpChannel(mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
                        pending_remote) override;

  // chromeos::sensors::mojom::SensorServiceNewDevicesObserver
  void OnNewDeviceAdded(
      int32_t iio_device_id,
      const std::vector<chromeos::sensors::mojom::DeviceType>& types) override;

 private:
  friend LightProviderMojoTest;

  struct LightData {
    LightData();
    ~LightData();

    // Something wrong with attributes of this light sensor or simply not needed
    // if true.
    bool ignored = false;
    std::optional<std::string> name;
    std::optional<bool> on_lid;

    // Temporarily stores the remote, waiting for its attributes information.
    // It'll be passed to LightProviderMojo' constructor as an argument after
    // all information is collected, if this sensor is needed.
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> remote;
  };

  void OnNewDevicesObserverDisconnect();
  // Timeout of new devices. If the target light sensor is still not present,
  // assumes it not supported, notifies observers with
  // AlsReader::AlsInitStatus::kIncorrectConfig, and fails the initialization.
  void OnNewDevicesTimeout();

  // Registers chromeos::sensors::mojom::SensorHalClient to Sensor Hal
  // Dispatcher, waiting for the Mojo connection to IIO Service.
  void RegisterSensorClient();
  void OnSensorHalClientFailure();

  void OnSensorServiceDisconnect();
  void ResetSensorService();

  // Called when an in-use device is unplugged, and we need to search for other
  // devices to use.
  // Assumes that the angle device won't be unplugged.
  void ResetStates();
  void QueryDevices();

  // Callback of GetDeviceIds(LIGHT), containing all iio_device_ids of light
  // sensors.
  void GetLightIdsCallback(const std::vector<int32_t>& light_ids);

  // Creates the Mojo channel for the light sensor, and requests the light
  // sensor's required attributes before determining which one to be used.
  void RegisterLightWithId(int32_t id);
  void GetNameLocationCallback(
      int32_t id,
      const std::vector<std::optional<std::string>>& values);

  // Ignores the light with |id| due to some errors of it's attributes.
  void IgnoreLight(int32_t id);

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> GetSensorDeviceRemote(
      int32_t id);
  void OnLightRemoteDisconnect(int32_t id,
                               uint32_t custom_reason_code,
                               const std::string& description);

  void DetermineLightSensor(int32_t id);
  void SetupLightSamplesObserver();

  // The Mojo channel connecting to Sensor Hal Dispatcher.
  mojo::Receiver<chromeos::sensors::mojom::SensorHalClient> sensor_hal_client_{
      this};

  // The Mojo channel to query and request for devices.
  mojo::Remote<chromeos::sensors::mojom::SensorService> sensor_service_remote_;

  // The Mojo channel to get notified when new devices are added to IIO Service.
  mojo::Receiver<chromeos::sensors::mojom::SensorServiceNewDevicesObserver>
      new_devices_observer_{this};

  // First is the light sensor's iio device id, second is it's data and mojo
  // remote.
  std::map<int32_t, LightData> lights_;

  bool als_init_status_set_ = false;
  // The device id of light to be used.
  std::optional<int32_t> light_device_id_;

  // The observer that waits for the wanted light sensor's samples and sends
  // them to |als_reader_|.
  std::unique_ptr<LightSamplesObserver> observer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LightProviderMojo> weak_ptr_factory_{this};
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_LIGHT_PROVIDER_MOJO_H_
