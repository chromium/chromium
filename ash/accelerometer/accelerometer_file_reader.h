// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_FILE_READER_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_FILE_READER_H_

#include <string>
#include <vector>

#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/files/file_util.h"
#include "base/observer_list_threadsafe.h"

namespace ash {

enum class State { INITIALIZING, SUCCESS, FAILED };

// Work that runs on a base::TaskRunner. It determines the accelerometer
// configuration, and reads the data. Upon a successful read it will notify
// all observers.
class AccelerometerFileReader : public AccelerometerProviderInterface,
                                public TabletModeObserver {
 public:
  AccelerometerFileReader();
  AccelerometerFileReader(const AccelerometerFileReader&) = delete;
  AccelerometerFileReader& operator=(const AccelerometerFileReader&) = delete;

  // AccelerometerProviderInterface:
  void PrepareAndInitialize(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) override;
  void AddObserver(AccelerometerReader::Observer* observer) override;
  void RemoveObserver(AccelerometerReader::Observer* observer) override;
  void StartListenToTabletModeController() override;
  void StopListenToTabletModeController() override;
  void SetEmitEvents(bool emit_events) override;

  // Attempts to read the accelerometer data. Upon success, converts the raw
  // reading to an AccelerometerUpdate and notifies observers. Triggers another
  // read at the current sampling rate.
  void Read();

  // Controls accelerometer reading.
  void EnableAccelerometerReading();
  void DisableAccelerometerReading();

  // Tracks if accelerometer initialization is completed.
  void CheckInitStatus();

  // With ChromeOS EC lid angle driver present, accelerometer read is cancelled
  // in clamshell mode, and triggered when entering tablet mode.
  void TriggerRead();
  void CancelRead();

  // TabletModeObserver:
  void OnTabletPhysicalStateChanged() override;

 private:
  // Represents necessary information in order to read an accelerometer device.
  struct ReadingData {
    ReadingData();
    ReadingData(const ReadingData&);
    ~ReadingData();

    // The full path to the accelerometer device to read.
    base::FilePath path;

    // The accelerometer sources which can be read from |path|.
    std::vector<AccelerometerSource> sources;
  };

  // Configuration structure for accelerometer device.
  struct ConfigurationData {
    ConfigurationData();
    ~ConfigurationData();

    // Number of accelerometers on device.
    size_t count;

    // sysfs entry to trigger readings.
    base::FilePath trigger_now;

    // Which accelerometers are present on device.
    bool has[ACCELEROMETER_SOURCE_COUNT];

    // Scale of accelerometers (i.e. raw value * scale = m/s^2).
    float scale[ACCELEROMETER_SOURCE_COUNT][3];

    // Index of each accelerometer axis in data stream.
    int index[ACCELEROMETER_SOURCE_COUNT][3];

    // The information for each accelerometer device to be read. In kernel 3.18
    // there is one per ACCELEROMETER_SOURCE_COUNT. On 3.14 there is only one.
    std::vector<ReadingData> reading_data;
  };

  ~AccelerometerFileReader() override;

  // Detects the accelerometer configuration.
  // If an accelerometer is available, it triggers reads.
  // This function MAY be called more than once.
  // This function contains the actual initialization code to be run by the
  // Initialize function. It is needed because on some devices the sensor hub
  // isn't available at the time the call to Initialize is made. If the sensor
  // is found to be missing we'll make a call to TryScheduleInitializeInternal.
  void InitializeInternal();

  // Attempt to reschedule a run of InitializeInternal(). The function will be
  // scheduled to run again if Now() < initialization_timeout_.
  void TryScheduleInitializeInternal();

  // When accelerometers are presented as separate iio_devices this will perform
  // the initialize for one of the devices, at the given |iio_path| and the
  // symbolic link |name|. |location| is defined by AccelerometerSource.
  bool InitializeAccelerometer(const base::FilePath& iio_path,
                               const base::FilePath& name,
                               const std::string& location);

  // TODO(jonross): Separate the initialization into separate files. Add a gyp
  // rule to have them built for the appropriate kernels. (crbug.com/525658)
  // When accelerometers are presented as a single iio_device this will perform
  // the initialization for both of them.
  bool InitializeLegacyAccelerometers(const base::FilePath& iio_path,
                                      const base::FilePath& name);

  // Attempts to read the accelerometer data. Upon a success, converts the raw
  // reading to an AccelerometerUpdate and notifies observers.
  void ReadFileAndNotify();

  void SetEmitEventsInternal(bool emit_events);

  // The current initialization state of reader.
  State initialization_state_ = State::INITIALIZING;

  // True if periodical accelerometer read is on.
  bool accelerometer_read_on_ = false;

  bool emit_events_ = true;

  // The time at which initialization re-tries should stop.
  base::TimeTicks initialization_timeout_;

  // The accelerometer configuration.
  ConfigurationData configuration_;

  // The observers to notify of accelerometer updates.
  scoped_refptr<base::ObserverListThreadSafe<AccelerometerReader::Observer>>
      observers_;

  // The task runner to use for blocking tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The last seen accelerometer data.
  scoped_refptr<AccelerometerUpdate> update_;
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_FILE_READER_H_
