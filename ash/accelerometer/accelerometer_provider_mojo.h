// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_PROVIDER_MOJO_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_PROVIDER_MOJO_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/accelerometer/accel_gyro_samples_observer.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/ash_export.h"
#include "base/sequence_checker.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// As devices may be late-present, the state and available devices cannot be
// determined within any given time or requests. This MojoState helps
// AccelerometerProviderMojo determine the current available devices and
// provides a clear finite state machine. States that should provide samples:
// LID, LID_BASE, ANGL_LID.
enum class MojoState {
  INITIALIZING,  // No devices available yet.
  LID,           // Only lid-accelerometer is available.
  BASE,          // Only base-accelerometer is available.
  LID_BASE,      // Both accelerometers are available.
  ANGL,          // Only lid-angle driver is available.
  ANGL_LID,      // Both lid-angle driver and lid-accelerometer are available.
};

class AccelerometerProviderMojoTest;

// Work that runs on the UI thread. As a sensor client, it communicates with IIO
// Service, determines the accelerometers' configuration, and waits for the
// accelerometers' samples. Upon receiving a sample, it will notify all
// observers.
class ASH_EXPORT AccelerometerProviderMojo
    : public AccelerometerProviderInterface,
      public chromeos::sensors::mojom::SensorHalClient,
      public chromeos::sensors::mojom::SensorServiceNewDevicesObserver {
 public:
  AccelerometerProviderMojo();
  AccelerometerProviderMojo(const AccelerometerProviderMojo&) = delete;
  AccelerometerProviderMojo& operator=(const AccelerometerProviderMojo&) =
      delete;

  // AccelerometerProviderInterface:
  void PrepareAndInitialize() override;
  void TriggerRead() override;
  void CancelRead() override;

  // chromeos::sensors::mojom::SensorHalClient:
  void SetUpChannel(mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
                        pending_remote) override;

  // chromeos::sensors::mojom::SensorServiceNewDevicesObserver
  void OnNewDeviceAdded(
      int32_t iio_device_id,
      const std::vector<chromeos::sensors::mojom::DeviceType>& types) override;

  MojoState GetInitializationStateForTesting() const;

 protected:
  // AccelerometerProviderInterface:
  bool ShouldDelayOnTabletPhysicalStateChanged() override;

 private:
  friend AccelerometerProviderMojoTest;

  struct AccelerometerData {
    AccelerometerData();
    ~AccelerometerData();

    bool ignored = false;
    // Temporarily stores the accelerometer remote, waiting for it's scale and
    // location information. It'll be passed to |samples_observer| as an
    // argument after all information is collected.
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> remote;
    std::optional<AccelerometerSource> location;
    std::optional<float> scale;
    std::unique_ptr<AccelGyroSamplesObserver> samples_observer;
  };

  ~AccelerometerProviderMojo() override;

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

  void SetECLidAngleDriverSupported();

  // Update |initialization_state_| upon new devices' arrival.

  // MojoState (|initialization_state_|) transition:
  //   INITIALIZING -> ANGL
  //   LID          -> ANGL_LID
  //   BASE         -> ANGL
  //   ANGL            Shouldn't happen
  //   ANGL_LID        Shouldn't happen
  void UpdateStateWithECLidAngleDriverSupported();

  // MojoState (|initialization_state_|) transition:
  //   INITIALIZING -> LID
  //   LID             Shouldn't happen
  //   BASE         -> LID_BASE
  //   ANGL         -> ANGL_LID
  //   ANGL_LID        Shouldn't happen
  void UpdateStateWithLidAccelerometer();

  // MojoState (|initialization_state_|) transition:
  //   INITIALIZING -> BASE
  //   LID          -> LID_BASE
  //   BASE            Shouldn't happen
  //   ANGL         -> ANGL
  //   ANGL_LID     -> ANGL_LID
  void UpdateStateWithBaseAccelerometer();

  void SetNewDevicesObserver();
  void OnNewDevicesObserverDisconnect();
  // Timeout of new devices. If lid-angle driver is still not present, assumes
  // it not supported and notifies observers.
  // This class still listens to new devices after the timeout to catch the
  // really late-present devices and avoid those issues, as the current use
  // cases/observers allow that.
  void OnNewDevicesTimeout();

  // Callback of GetDeviceIds(ANGL), containing the lid-angle device's id if it
  // exists.
  void GetLidAngleIdsCallback(const std::vector<int32_t>& lid_angle_ids);

  // Callback of GetDeviceIds(ACCEL), containing all iio_device_ids of
  // accelerometers.
  void GetAccelerometerIdsCallback(
      const std::vector<int32_t>& accelerometer_ids);

  // Creates the Mojo channel for the accelerometer, and requests the
  // accelerometer's required attributes before creating the
  // AccelGyroSamplesObserver of it.
  void RegisterAccelerometerWithId(int32_t id);
  void OnAccelerometerRemoteDisconnect(int32_t id,
                                       uint32_t custom_reason_code,
                                       const std::string& description);
  void GetAttributesCallback(
      int32_t id,
      const std::vector<std::optional<std::string>>& values);

  // Ignores the accelerometer as the attributes are not expected.
  void IgnoreAccelerometer(int32_t id);

  // Creates the AccelGyroSamplesObserver for the accelerometer with |id|.
  void CreateAccelerometerSamplesObserver(int32_t id);

  // Controls accelerometer reading.
  void EnableAccelerometerReading();
  void DisableAccelerometerReading();

  // Called by |AccelerometerData::samples_observer| stored in the
  // |accelerometers_| map, containing a sample of the accelerometer.
  void OnSampleUpdatedCallback(int iio_device_id, std::vector<float> sample);

  // The state that contains the information of devices we have now. Used for
  // late-present devices.
  MojoState initialization_state_ = MojoState::INITIALIZING;

  // The Mojo channel connecting to Sensor Hal Dispatcher.
  mojo::Receiver<chromeos::sensors::mojom::SensorHalClient> sensor_hal_client_{
      this};

  // The Mojo channel to query and request for devices.
  mojo::Remote<chromeos::sensors::mojom::SensorService> sensor_service_remote_;

  // The Mojo channel to get notified when new devices are added to IIO Service.
  mojo::Receiver<chromeos::sensors::mojom::SensorServiceNewDevicesObserver>
      new_devices_observer_{this};

  // First is the accelerometer's iio device id, second is it's data, mojo
  // remote and samples observer.
  std::map<int32_t, AccelerometerData> accelerometers_;

  // First is the location index, second is the id of the accelerometer being
  // used in this reader.
  std::map<AccelerometerSource, int32_t> location_to_accelerometer_id_;

  // The flag to delay |OnTabletPhysicalStateChanged| until
  // |ec_lid_angle_driver_status_| is set.
  bool pending_on_tablet_physical_state_changed_ = false;

  // True if periodical accelerometer read is on.
  bool accelerometer_read_on_ = false;

  // The last seen accelerometer data.
  AccelerometerUpdate update_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_PROVIDER_MOJO_H_
