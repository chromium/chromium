// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SENSOR_INFO_SENSOR_PROVIDER_H_
#define ASH_SENSOR_INFO_SENSOR_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/accelerometer/accel_gyro_samples_observer.h"
#include "ash/accelerometer/accelerometer_constants.h"
#include "ash/ash_export.h"
#include "ash/sensor_info/sensor_types.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class SensorProviderTest;

// SensorProvider communicates with IIO Service, and provide information from
// three types of sensors: accelerometers, gyroscopes, lid_angle sensors.
// SensorProvider determines sensors' configuration, and waits for the
// sensors' samples. Upon receiving a sample, it will notify all
// observers.
class ASH_EXPORT SensorProvider
    : public chromeos::sensors::mojom::SensorHalClient,
      public chromeos::sensors::mojom::SensorServiceNewDevicesObserver {
 public:
  // Class tracking existence of sensors.
  class DeviceState {
   public:
    DeviceState();
    ~DeviceState();

    DeviceState(const DeviceState&) = delete;
    DeviceState& operator=(const DeviceState&) = delete;

    // Returns true if all kinds of sensors available.
    bool AllSensorsFound() const;

    void Reset();

    // Returns true if all present sensors have update.
    bool CompareUpdate(SensorUpdate update) const;

    // Adds a type of sensor.
    void AddSource(SensorType source);

    // Removes a type of sensor.
    void RemoveSource(SensorType source);

    // Gets the existence of `source`.
    bool GetSource(SensorType source) const;

    // Gets DeviceState.present_ while testing.
    std::vector<bool> GetStatesForTesting() const;

   private:
    // Vector storing the existence of types of sensors.
    std::vector<bool> present_;
  };

  // Struct tracking all sensor info.
  struct SensorData {
    SensorData();
    ~SensorData();

    // Sensor marked ignored will not be used anymore.
    bool ignored = false;
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> remote;
    std::optional<SensorLocation> location;
    // Data scale of this sensor (Used for initializing Samples_Observer,
    // range:(0, 10)). Will be 1.0 for most of the cases.
    std::optional<float> scale;

    // SamplesObserver of this sensor.
    std::unique_ptr<ash::AccelGyroSamplesObserver> samples_observer;
  };

  SensorProvider();

  SensorProvider(const SensorProvider&) = delete;
  SensorProvider& operator=(const SensorProvider&) = delete;

  ~SensorProvider() override;

  // chromeos::sensors::mojom::SensorHalClient:
  void SetUpChannel(mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
                        pending_remote) override;

  // chromeos::sensors::mojom::SensorServiceNewDevicesObserver:
  void OnNewDeviceAdded(
      int32_t iio_device_id,
      const std::vector<chromeos::sensors::mojom::DeviceType>& types) override;

  // Adds/Removes SensorObservers.
  void AddObserver(SensorObserver* observer);
  void RemoveObserver(SensorObserver* observer);

  // Starts/Stops sensor reading.
  // Changes 'sensor_read_on_' and call EnableSensorReadingInternal.
  void EnableSensorReading();
  void StopSensorReading();

  // Gets DeviceState.state while testing.
  std::vector<bool> GetStateForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(SensorProviderTest, CheckNoScale);
  FRIEND_TEST_ALL_PREFIXES(SensorProviderTest, CheckNoLocation);
  FRIEND_TEST_ALL_PREFIXES(SensorProviderTest, GetSamplesOfLidAccel);
  FRIEND_TEST_ALL_PREFIXES(SensorProviderTest, GetSamplesOfLidAngleAndLidAccel);
  FRIEND_TEST_ALL_PREFIXES(SensorProviderTest, GetSamplesOfBaseGyroscope);
  FRIEND_TEST_ALL_PREFIXES(SensorProviderTest,
                           GetSamplesOfBaseGyroscopeAndBaseAccel);
  FRIEND_TEST_ALL_PREFIXES(SensorProviderTest, AddSensorsWhileSampling);
  FRIEND_TEST_ALL_PREFIXES(SensorProviderTest, GetSamplesOfAccelGyroDevices);

  // Gets all existing sensor devices then initialize sensor devices.
  void QueryDevices();

  // Callback of GetDeviceIds(DeviceType::ANGL), containing the lid_angle
  // device's id if it exists.
  void OnLidAngleIds(const std::vector<int32_t>& lid_angle_ids);

  // Callback of GetDeviceIds(DeviceType::ACCEL), containing all iio_device_ids
  // of accelerometers.
  void OnAccelerometerIds(const std::vector<int32_t>& accelerometer_ids);

  // Callback of GetDeviceIds(DeviceType::ANGLVEL), containing all
  // iio_device_ids of gyroscopes.
  void OnGyroscopeIds(const std::vector<int32_t>& gyroscope_ids);

  // Registers `chromeos::sensors::mojom::SensorHalClient` to Sensor Hal
  // Dispatcher, waiting for the Mojo connection to IIO Service.
  void RegisterSensorClient();
  void OnSensorServiceDisconnect();
  void OnSensorHalClientFailure();
  void ResetSensorService();
  void ResetStates();

  // Waits for new devices from IIO Service.
  void SetNewDevicesObserver();
  void OnNewDevicesObserverDisconnect();

  // Creates the Mojo channel for the sensor, and requests the
  // sensor's location and scale attributes before creating the
  // SensorSamplesObserver of it.
  void RegisterSensor(chromeos::sensors::mojom::DeviceType device_type,
                      int32_t id);

  // Ignores the sensor with id, device_type as the attributes are not expected.
  void IgnoreSensor(chromeos::sensors::mojom::DeviceType device_type,
                    int32_t id);

  // Enables all SamplesObservers and sets frequency for SamplesObservers.
  void EnableSensorReadingInternal();

  void OnSensorRemoteDisconnect(
      chromeos::sensors::mojom::DeviceType device_type,
      int32_t id,
      uint32_t custom_reason_code,
      const std::string& description);

  // Callback for getting lid angle value.
  void OnLidAngleValue(const std::vector<std::optional<std::string>>& values);

  // Callback for getting sensor's location and scale attributes.
  void OnAttributes(chromeos::sensors::mojom::DeviceType device_type,
                    int32_t id,
                    const std::vector<std::optional<std::string>>& values);

  // Creates the SamplesObserver for the sensor with 'id'.
  void CreateSensorSamplesObserver(
      chromeos::sensors::mojom::DeviceType device_type,
      int32_t id);

  // Called by `SensorData::samples_observer` stored in the
  // `sensors_` map, containing a sample of the sensor.
  void OnSampleUpdatedCallback(chromeos::sensors::mojom::DeviceType device_type,
                               int iio_device_id,
                               std::vector<float> sample);

  // Notifies external observer updates.
  void NotifySensorUpdated(SensorUpdate update);

  // Returns true if all stored SensorData (excluding SensorData for lid_angle
  // and SensorData for ignored devices) have a SamplesObserver.
  bool CheckSensorSamplesObserver();

  // Calls GetChannelsAttributesCallback to get lid_angle update.
  void GetLidAngleUpdate();

  // Stores <SensorType, device_id> pairs.
  std::map<SensorType, int32_t> type_to_sensor_id_;

  // Map storing <device_id, <chromeos::sensors::mojom::DeviceType,
  // SensorData>>.
  std::map<int32_t, std::map<chromeos::sensors::mojom::DeviceType, SensorData>>
      sensors_;

  DeviceState current_state_;

  // The Mojo channel connecting to Sensor Hal Dispatcher. SensorHalDispatcher
  // will receive registrations of SensorService.
  mojo::Receiver<chromeos::sensors::mojom::SensorHalClient> sensor_hal_client_{
      this};

  // The Mojo channel to query and request for devices.
  mojo::Remote<chromeos::sensors::mojom::SensorService> sensor_service_remote_;

  // The Mojo channel to get notified when new devices are added to IIO Service.
  mojo::Receiver<chromeos::sensors::mojom::SensorServiceNewDevicesObserver>
      new_devices_observer_{this};

  // The last seen sensor data.
  SensorUpdate update_;

  // Whether send updates to external observers. Value is only changed by
  // external calls of EnableSensorReading() and StopSensorReading().
  bool sensor_read_on_ = false;

  // List of all external observers.
  base::ObserverList<SensorObserver> observers_;

  base::WeakPtrFactory<SensorProvider> weak_ptr_factory_{this};
};

}  // namespace ash
#endif  // ASH_SENSOR_INFO_SENSOR_PROVIDER_H_
