// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_PROVIDER_MOJO_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_PROVIDER_MOJO_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_samples_observer.h"
#include "ash/ash_export.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Work that runs on the UI thread. As a sensor client, it communicates with IIO
// Service, determines the accelerometers' configuration, and waits for the
// accelerometers' samples. Upon receiving a sample, it will notify all
// observers.
class ASH_EXPORT AccelerometerProviderMojo
    : public AccelerometerProviderInterface,
      public chromeos::sensors::mojom::SensorHalClient {
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

  State GetInitializationStateForTesting() const;

 protected:
  // AccelerometerProviderInterface:
  bool ShouldDelayOnTabletPhysicalStateChanged() override;

 private:
  struct AccelerometerData {
    AccelerometerData();
    ~AccelerometerData();

    bool ignored = false;
    // Temporarily stores the accelerometer remote, waiting for it's scale and
    // location information. It'll be passed to |samples_observer| as an
    // argument after all information is collected.
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> remote;
    base::Optional<AccelerometerSource> location;
    base::Optional<float> scale;
    std::unique_ptr<AccelerometerSamplesObserver> samples_observer;
  };

  ~AccelerometerProviderMojo() override;

  // Registers chromeos::sensors::mojom::SensorHalClient to Sensor Hal
  // Dispatcher, waiting for the Mojo connection to IIO Service.
  void RegisterSensorClient();
  void OnSensorHalClientFailure();

  void OnSensorServiceDisconnect();
  void ResetSensorService();

  // Callback of GetDeviceIds(ANGL), containing the lid-angle device's id if it
  // exists.
  void GetLidAngleIdsCallback(const std::vector<int32_t>& lid_angle_ids);

  // Callback of GetDeviceIds(ACCEL), containing all iio_device_ids of
  // accelerometers.
  void GetAccelerometerIdsCallback(
      const std::vector<int32_t>& accelerometer_ids);

  // Creates the Mojo channel for the accelerometer, and requests the
  // accelerometer's required attributes before creating the
  // AccelerometerSamplesObserver of it.
  void RegisterAccelerometerWithId(int32_t id);
  void OnAccelerometerRemoteDisconnect(int32_t id);
  void GetAttributesCallback(
      int32_t id,
      const std::vector<base::Optional<std::string>>& values);

  // Ignores the accelerometer as the attributes are not expected.
  void IgnoreAccelerometer(int32_t id);
  // Checks and sets |initialization_state_| if all information is retrieved.
  void CheckInitialization();

  // Creates the AccelerometerSamplesObserver for the accelerometer with |id|.
  void CreateAccelerometerSamplesObserver(int32_t id);

  // Controls accelerometer reading.
  void EnableAccelerometerReading();
  void DisableAccelerometerReading();

  // Called by |AccelerometerData::samples_observer| stored in the
  // |accelerometers_| map, containing a sample of the accelerometer.
  void OnSampleUpdatedCallback(int iio_device_id, std::vector<float> sample);

  // Sets FAILED to |initialization_state_| due to an error.
  void FailedToInitialize();

  // The Mojo channel connecting to Sensor Hal Dispatcher.
  mojo::Receiver<chromeos::sensors::mojom::SensorHalClient> sensor_hal_client_{
      this};

  // The Mojo channel to query and request for devices.
  mojo::Remote<chromeos::sensors::mojom::SensorService> sensor_service_remote_;

  // The existence of the accelerometer on the base.
  bool has_accelerometer_base_ = false;

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
