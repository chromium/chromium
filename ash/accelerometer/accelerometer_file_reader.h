// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELEROMETER_ACCELEROMETER_FILE_READER_H_
#define ASH_ACCELEROMETER_ACCELEROMETER_FILE_READER_H_

#include <string>
#include <vector>

#include "ash/accelerometer/accelerometer_reader.h"
#include "base/files/file_util.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace ash {

enum class State { kInitializing, kSuccess, kFailed };

// Work that runs on a base::TaskRunner. It determines the accelerometer
// configuration, and reads the data. Upon a successful read it will notify
// all observers.
class AccelerometerFileReader : public AccelerometerProviderInterface {
 public:
  AccelerometerFileReader();
  AccelerometerFileReader(const AccelerometerFileReader&) = delete;
  AccelerometerFileReader& operator=(const AccelerometerFileReader&) = delete;

  // AccelerometerProviderInterface:
  void PrepareAndInitialize() override;
  void TriggerRead() override;
  void CancelRead() override;

 private:
  struct InitializationResult {
    InitializationResult();
    ~InitializationResult();

    State initialization_state;
    ECLidAngleDriverStatus ec_lid_angle_driver_status;
  };

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

  // Post a task to initialize on |blocking_task_runner_| and process the result
  // on the UI thread. May be called multiple times in the retries.
  void TryScheduleInitialize();

  // Detects the accelerometer configuration in |blocking_task_runner_|.
  // If an accelerometer is available, it triggers reads.
  // This function MAY be called more than once.
  // This function contains the actual initialization code to be run by the
  // Initialize function. It is needed because on some devices the sensor hub
  // isn't available at the time the call to Initialize is made.
  // If the sensor is found to be missing we'll request a re-run of this
  // function by returning State::kInitializing. TryScheduleInitializeInternal.
  InitializationResult InitializeInternal();

  // Attempt to finish the initialization with the result state. If it's
  // State::kInitializing, it means something is missing and need to re-run
  // |TryScheduleInitialize|, if not timed out yet.
  void SetStatesWithInitializationResult(InitializationResult result);

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

  // Controls accelerometer reading.
  void EnableAccelerometerReading();
  void DisableAccelerometerReading();

  // The current initialization state of reader.
  State initialization_state_ = State::kInitializing;

  // Attempts to read the accelerometer data in |blocking_task_runner_|. Upon a
  // success, converts the raw reading to an AccelerometerUpdate and notifies
  // observers.
  void ReadSample();

  // The time at which initialization re-tries should stop.
  base::TimeTicks initialization_timeout_;

  // The accelerometer configuration.
  // Only used in |blocking_task_runner_|.
  ConfigurationData configuration_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The timer to repeatedly read samples.
  // Only used in |blocking_task_runner_|.
  base::RepeatingTimer read_refresh_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The task runner to use for blocking tasks.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // ASH_ACCELEROMETER_ACCELEROMETER_FILE_READER_H_
