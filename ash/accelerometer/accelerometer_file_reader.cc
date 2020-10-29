// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_file_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/numerics/math_constants.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

namespace {

// Paths to access necessary data from the accelerometer device.
constexpr base::FilePath::CharType kAccelerometerDevicePath[] =
    FILE_PATH_LITERAL("/dev/cros-ec-accel");
constexpr base::FilePath::CharType kAccelerometerIioBasePath[] =
    FILE_PATH_LITERAL("/sys/bus/iio/devices/");

// Paths to ChromeOS EC lid angle driver.
constexpr base::FilePath::CharType kECLidAngleDriverPath[] =
    FILE_PATH_LITERAL("/sys/bus/platform/drivers/cros-ec-lid-angle/");

// Trigger created by accelerometer-init.sh to query the sensors.
constexpr char kTriggerPrefix[] = "trigger";
constexpr char kTriggerName[] = "sysfstrig0\n";

// Sysfs entry to trigger readings.
constexpr base::FilePath::CharType kTriggerNow[] = "trigger_now";

// This is the per source scale file in use on kernels older than 3.18. We
// should remove this when all devices having accelerometers are on kernel 3.18
// or later or have been patched to use new format: http://crbug.com/510831
constexpr base::FilePath::CharType kLegacyScaleNameFormatString[] =
    "in_accel_%s_scale";

// File within kAccelerometerDevicePath/device* which denotes a single scale to
// be used across all axes.
constexpr base::FilePath::CharType kAccelerometerScaleFileName[] = "scale";

// File within kAccelerometerDevicePath/device* which denotes the
// AccelerometerSource for the accelerometer.
constexpr base::FilePath::CharType kAccelerometerLocationFileName[] =
    "location";

// The filename giving the path to read the scan index of each accelerometer
// axis.
constexpr char kLegacyAccelerometerScanIndexPathFormatString[] =
    "scan_elements/in_accel_%s_%s_index";

// The filename giving the path to read the scan index of each accelerometer
// when they are separate device paths.
constexpr char kAccelerometerScanIndexPathFormatString[] =
    "scan_elements/in_accel_%s_index";

// The names of the accelerometers. Matches up with the enum AccelerometerSource
// in ash/accelerometer/accelerometer_types.h.
constexpr char kAccelerometerNames[ACCELEROMETER_SOURCE_COUNT][5] = {"lid",
                                                                     "base"};

// The axes on each accelerometer. The order was changed on kernel 3.18+.
constexpr char kAccelerometerAxes[][2] = {"x", "y", "z"};
constexpr char kLegacyAccelerometerAxes[][2] = {"y", "x", "z"};

// The length required to read uint values from configuration files.
constexpr size_t kMaxAsciiUintLength = 21;

// The size of individual values.
constexpr size_t kDataSize = 2;

// The number of axes for which there are acceleration readings.
constexpr int kNumberOfAxes = 3;

// The size of data in one reading of the accelerometers.
constexpr int kSizeOfReading = kDataSize * kNumberOfAxes;

// The time to wait between reading the accelerometer.
constexpr base::TimeDelta kDelayBetweenReads =
    base::TimeDelta::FromMilliseconds(100);

// The TimeDelta before giving up on initialization. This is needed because the
// sensor hub might not be online when the Initialize function is called.
constexpr base::TimeDelta kInitializeTimeout = base::TimeDelta::FromSeconds(5);

// The time between initialization checks.
constexpr base::TimeDelta kDelayBetweenInitChecks =
    base::TimeDelta::FromMilliseconds(500);

// Reads |path| to the unsigned int pointed to by |value|. Returns true on
// success or false on failure.
bool ReadFileToInt(const base::FilePath& path, int* value) {
  std::string s;
  DCHECK(value);
  if (!base::ReadFileToStringWithMaxSize(path, &s, kMaxAsciiUintLength))
    return false;

  base::TrimWhitespaceASCII(s, base::TRIM_ALL, &s);
  if (!base::StringToInt(s, value)) {
    LOG(ERROR) << "Failed to parse int \"" << s << "\" from " << path.value();
    return false;
  }
  return true;
}

// Reads |path| to the double pointed to by |value|. Returns true on success or
// false on failure.
bool ReadFileToDouble(const base::FilePath& path, double* value) {
  std::string s;
  DCHECK(value);
  if (!base::ReadFileToString(path, &s))
    return false;

  base::TrimWhitespaceASCII(s, base::TRIM_ALL, &s);
  if (!base::StringToDouble(s, value)) {
    LOG(ERROR) << "Failed to parse double \"" << s << "\" from "
               << path.value();
    return false;
  }
  return true;
}

}  // namespace

AccelerometerFileReader::AccelerometerFileReader()
    : observers_(
          new base::ObserverListThreadSafe<AccelerometerReader::Observer>()) {}

void AccelerometerFileReader::PrepareAndInitialize(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
  task_runner_ = sequenced_task_runner;

  initialization_state_ = State::INITIALIZING;

  initialization_timeout_ = base::TimeTicks::Now() + kInitializeTimeout;

  // Asynchronously detect and initialize the accelerometer to avoid delaying
  // startup.
  task_runner_->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&AccelerometerFileReader::InitializeInternal, this));
}

void AccelerometerFileReader::TryScheduleInitializeInternal() {
  // If we haven't yet passed the timeout cutoff, try this again. This will
  // be scheduled at the same rate as reading.
  if (base::TimeTicks::Now() < initialization_timeout_) {
    DCHECK_EQ(State::INITIALIZING, initialization_state_);
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AccelerometerFileReader::InitializeInternal, this),
        kDelayBetweenReads);
  } else {
    LOG(ERROR) << "Failed to initialize for accelerometer read.\n";
    initialization_state_ = State::FAILED;
  }
}

void AccelerometerFileReader::InitializeInternal() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  DCHECK_EQ(State::INITIALIZING, initialization_state_);

  // Check for accelerometer symlink which will be created by the udev rules
  // file on detecting the device.
  if (base::IsDirectoryEmpty(base::FilePath(kAccelerometerDevicePath))) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      LOG(WARNING) << "Accelerometer device directory is empty at "
                   << kAccelerometerDevicePath;
      TryScheduleInitializeInternal();
    } else {
      initialization_state_ = State::FAILED;
    }
    return;
  }

  if (base::SysInfo::IsRunningOnChromeOS() &&
      !base::IsDirectoryEmpty(base::FilePath(kECLidAngleDriverPath))) {
    ec_lid_angle_driver_status_ = ECLidAngleDriverStatus::SUPPORTED;
  } else {
    ec_lid_angle_driver_status_ = ECLidAngleDriverStatus::NOT_SUPPORTED;
  }

  // Find trigger to use:
  base::FileEnumerator trigger_dir(base::FilePath(kAccelerometerIioBasePath),
                                   false, base::FileEnumerator::DIRECTORIES);
  std::string prefix = kTriggerPrefix;
  for (base::FilePath name = trigger_dir.Next(); !name.empty();
       name = trigger_dir.Next()) {
    if (name.BaseName().value().substr(0, prefix.size()) != prefix)
      continue;
    std::string trigger_name;
    if (!base::ReadFileToString(name.Append("name"), &trigger_name)) {
      if (base::SysInfo::IsRunningOnChromeOS()) {
        LOG(WARNING) << "Unable to read the trigger name at " << name.value();
      }
      continue;
    }
    if (trigger_name == kTriggerName) {
      base::FilePath trigger_now = name.Append(kTriggerNow);
      if (!base::PathExists(trigger_now)) {
        if (base::SysInfo::IsRunningOnChromeOS()) {
          LOG(ERROR) << "Accelerometer trigger does not exist at "
                     << trigger_now.value();
        }
        initialization_state_ = State::FAILED;
        return;
      } else {
        configuration_.trigger_now = trigger_now;
        break;
      }
    }
  }
  if (configuration_.trigger_now.empty()) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      LOG(ERROR) << "Accelerometer trigger not found";
      TryScheduleInitializeInternal();
    } else {
      initialization_state_ = State::FAILED;
    }
    return;
  }

  base::FileEnumerator symlink_dir(base::FilePath(kAccelerometerDevicePath),
                                   false, base::FileEnumerator::FILES);
  bool legacy_cross_accel = false;
  for (base::FilePath name = symlink_dir.Next(); !name.empty();
       name = symlink_dir.Next()) {
    base::FilePath iio_device;
    if (!base::ReadSymbolicLink(name, &iio_device)) {
      LOG(ERROR) << "Failed to read symbolic link " << kAccelerometerDevicePath
                 << "/" << name.MaybeAsASCII() << "\n";
      initialization_state_ = State::FAILED;
      return;
    }

    base::FilePath iio_path(base::FilePath(kAccelerometerIioBasePath)
                                .Append(iio_device.BaseName()));
    std::string location;
    legacy_cross_accel = !base::ReadFileToString(
        base::FilePath(iio_path).Append(kAccelerometerLocationFileName),
        &location);
    if (legacy_cross_accel) {
      if (!InitializeLegacyAccelerometers(iio_path, name)) {
        initialization_state_ = State::FAILED;
        return;
      }
    } else {
      base::TrimWhitespaceASCII(location, base::TRIM_ALL, &location);
      if (!InitializeAccelerometer(iio_path, name, location)) {
        initialization_state_ = State::FAILED;
        return;
      }
    }
  }

  // Verify indices are within bounds.
  for (int i = 0; i < ACCELEROMETER_SOURCE_COUNT; ++i) {
    if (!configuration_.has[i])
      continue;
    for (int j = 0; j < 3; ++j) {
      if (configuration_.index[i][j] < 0 ||
          configuration_.index[i][j] >=
              3 * static_cast<int>(configuration_.count)) {
        const char* axis = legacy_cross_accel ? kLegacyAccelerometerAxes[j]
                                              : kAccelerometerAxes[j];
        LOG(ERROR) << "Field index for " << kAccelerometerNames[i] << " "
                   << axis << " axis out of bounds.";
        initialization_state_ = State::FAILED;
        return;
      }
    }
  }

  initialization_state_ = State::SUCCESS;

  // If ChromeOS lid angle driver is not present, start accelerometer read and
  // read is always on.
  if (ec_lid_angle_driver_status_ == ECLidAngleDriverStatus::NOT_SUPPORTED)
    EnableAccelerometerReading();
}

void AccelerometerFileReader::Read() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  if (!accelerometer_read_on_)
    return;

  ReadFileAndNotify();
  task_runner_->PostNonNestableDelayedTask(
      FROM_HERE, base::BindOnce(&AccelerometerFileReader::Read, this),
      kDelayBetweenReads);
}

void AccelerometerFileReader::EnableAccelerometerReading() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  if (accelerometer_read_on_)
    return;

  accelerometer_read_on_ = true;
  Read();
}

void AccelerometerFileReader::DisableAccelerometerReading() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  if (!accelerometer_read_on_)
    return;

  accelerometer_read_on_ = false;
}

void AccelerometerFileReader::CheckInitStatus() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  switch (initialization_state_) {
    case State::SUCCESS:
      EnableAccelerometerReading();
      break;
    case State::FAILED:
      LOG(ERROR) << "Failed to initialize for accelerometer read.\n";
      break;
    case State::INITIALIZING:
      task_runner_->PostNonNestableDelayedTask(
          FROM_HERE,
          base::BindOnce(&AccelerometerFileReader::CheckInitStatus, this),
          kDelayBetweenInitChecks);
      break;
  }
}

void AccelerometerFileReader::TriggerRead() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  switch (initialization_state_) {
    case State::SUCCESS:
      if (ec_lid_angle_driver_status_ == ECLidAngleDriverStatus::SUPPORTED)
        EnableAccelerometerReading();
      break;
    case State::FAILED:
      LOG(ERROR) << "Failed to initialize for accelerometer read.\n";
      break;
    case State::INITIALIZING:
      task_runner_->PostNonNestableTask(
          FROM_HERE,
          base::BindOnce(&AccelerometerFileReader::CheckInitStatus, this));
      break;
  }
}

void AccelerometerFileReader::CancelRead() {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  if (initialization_state_ == State::SUCCESS &&
      ec_lid_angle_driver_status_ == ECLidAngleDriverStatus::SUPPORTED)
    DisableAccelerometerReading();
}

void AccelerometerFileReader::AddObserver(
    AccelerometerReader::Observer* observer) {
  observers_->AddObserver(observer);
  if (initialization_state_ == State::SUCCESS) {
    task_runner_->PostNonNestableTask(
        FROM_HERE,
        base::BindOnce(&AccelerometerFileReader::ReadFileAndNotify, this));
  }
}

void AccelerometerFileReader::RemoveObserver(
    AccelerometerReader::Observer* observer) {
  observers_->RemoveObserver(observer);
}

void AccelerometerFileReader::StartListenToTabletModeController() {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

void AccelerometerFileReader::StopListenToTabletModeController() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void AccelerometerFileReader::SetEmitEvents(bool emit_events) {
  task_runner_->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&AccelerometerFileReader::SetEmitEventsInternal,
                                this, emit_events));
}

void AccelerometerFileReader::SetEmitEventsInternal(bool emit_events) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  emit_events_ = emit_events;
}

void AccelerometerFileReader::OnTabletPhysicalStateChanged() {
  // When CrOS EC lid angle driver is not present, accelerometer read is always
  // ON and can't be tuned. Thus AccelerometerFileReader no longer listens to
  // tablet mode event.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  if (ec_lid_angle_driver_status_ == ECLidAngleDriverStatus::NOT_SUPPORTED) {
    tablet_mode_controller->RemoveObserver(this);
    return;
  }

  // Auto rotation is turned on when the device is physically used as a tablet
  // (i.e. flipped or detached), regardless of the UI state (i.e. whether tablet
  // mode is turned on or off).
  const bool is_auto_rotation_on =
      tablet_mode_controller->is_in_tablet_physical_state();

  task_runner_->PostNonNestableTask(
      FROM_HERE,
      is_auto_rotation_on
          ? base::BindOnce(&AccelerometerFileReader::TriggerRead, this)
          : base::BindOnce(&AccelerometerFileReader::CancelRead, this));
}

AccelerometerFileReader::~AccelerometerFileReader() = default;

bool AccelerometerFileReader::InitializeAccelerometer(
    const base::FilePath& iio_path,
    const base::FilePath& name,
    const std::string& location) {
  size_t config_index = 0;
  for (; config_index < base::size(kAccelerometerNames); ++config_index) {
    if (location == kAccelerometerNames[config_index])
      break;
  }

  if (config_index >= base::size(kAccelerometerNames)) {
    LOG(ERROR) << "Unrecognized location: " << location << " for device "
               << name.MaybeAsASCII() << "\n";
    return false;
  }

  double scale;
  if (!ReadFileToDouble(iio_path.Append(kAccelerometerScaleFileName), &scale))
    return false;

  const int kNumberAxes = base::size(kAccelerometerAxes);
  for (size_t i = 0; i < kNumberAxes; ++i) {
    std::string accelerometer_index_path = base::StringPrintf(
        kAccelerometerScanIndexPathFormatString, kAccelerometerAxes[i]);
    if (!ReadFileToInt(iio_path.Append(accelerometer_index_path.c_str()),
                       &(configuration_.index[config_index][i]))) {
      LOG(ERROR) << "Index file " << accelerometer_index_path
                 << " could not be parsed\n";
      return false;
    }
    configuration_.scale[config_index][i] = scale;
  }
  configuration_.has[config_index] = true;
  configuration_.count++;

  ReadingData reading_data;
  reading_data.path =
      base::FilePath(kAccelerometerDevicePath).Append(name.BaseName());
  reading_data.sources.push_back(
      static_cast<AccelerometerSource>(config_index));

  configuration_.reading_data.push_back(reading_data);

  return true;
}

bool AccelerometerFileReader::InitializeLegacyAccelerometers(
    const base::FilePath& iio_path,
    const base::FilePath& name) {
  ReadingData reading_data;
  reading_data.path =
      base::FilePath(kAccelerometerDevicePath).Append(name.BaseName());
  // Read configuration of each accelerometer axis from each accelerometer from
  // /sys/bus/iio/devices/iio:deviceX/.
  for (size_t i = 0; i < base::size(kAccelerometerNames); ++i) {
    configuration_.has[i] = false;
    // Read scale of accelerometer.
    std::string accelerometer_scale_path = base::StringPrintf(
        kLegacyScaleNameFormatString, kAccelerometerNames[i]);
    // Read the scale for all axes.
    int scale_divisor = 0;
    if (!ReadFileToInt(iio_path.Append(accelerometer_scale_path.c_str()),
                       &scale_divisor)) {
      continue;
    }
    if (scale_divisor == 0) {
      LOG(ERROR) << "Accelerometer " << accelerometer_scale_path
                 << "has scale of 0 and will not be used.";
      continue;
    }

    configuration_.has[i] = true;
    for (size_t j = 0; j < base::size(kLegacyAccelerometerAxes); ++j) {
      configuration_.scale[i][j] = base::kMeanGravityFloat / scale_divisor;
      std::string accelerometer_index_path = base::StringPrintf(
          kLegacyAccelerometerScanIndexPathFormatString,
          kLegacyAccelerometerAxes[j], kAccelerometerNames[i]);
      if (!ReadFileToInt(iio_path.Append(accelerometer_index_path.c_str()),
                         &(configuration_.index[i][j]))) {
        configuration_.has[i] = false;
        LOG(ERROR) << "Index file " << accelerometer_index_path
                   << " could not be parsed\n";
        return false;
      }
    }
    if (configuration_.has[i]) {
      configuration_.count++;
      reading_data.sources.push_back(static_cast<AccelerometerSource>(i));
    }
  }

  // Adjust the directions of accelerometers to match the AccelerometerUpdate
  // type specified in ash/accelerometer/accelerometer_types.h.
  configuration_.scale[ACCELEROMETER_SOURCE_SCREEN][1] *= -1.0f;
  configuration_.scale[ACCELEROMETER_SOURCE_SCREEN][2] *= -1.0f;

  configuration_.reading_data.push_back(reading_data);
  return true;
}

void AccelerometerFileReader::ReadFileAndNotify() {
  DCHECK_EQ(State::SUCCESS, initialization_state_);

  // Initiate the trigger to read accelerometers simultaneously.
  int bytes_written = base::WriteFile(configuration_.trigger_now, "1\n", 2);
  if (bytes_written < 2) {
    PLOG(ERROR) << "Accelerometer trigger failure: " << bytes_written;
    return;
  }

  // Read resulting sample from /dev/cros-ec-accel.
  update_ = new AccelerometerUpdate();
  for (auto reading_data : configuration_.reading_data) {
    int reading_size = reading_data.sources.size() * kSizeOfReading;
    DCHECK_GT(reading_size, 0);
    char reading[reading_size];
    int bytes_read = base::ReadFile(reading_data.path, reading, reading_size);
    if (bytes_read < reading_size) {
      LOG(ERROR) << "Accelerometer Read " << bytes_read << " byte(s), expected "
                 << reading_size << " bytes from accelerometer "
                 << reading_data.path.MaybeAsASCII();
      return;
    }
    for (AccelerometerSource source : reading_data.sources) {
      DCHECK(configuration_.has[source]);
      int16_t* values = reinterpret_cast<int16_t*>(reading);
      update_->Set(source,
                   values[configuration_.index[source][0]] *
                       configuration_.scale[source][0],
                   values[configuration_.index[source][1]] *
                       configuration_.scale[source][1],
                   values[configuration_.index[source][2]] *
                       configuration_.scale[source][2]);
    }
  }

  if (!emit_events_)
    return;

  observers_->Notify(FROM_HERE,
                     &AccelerometerReader::Observer::OnAccelerometerUpdated,
                     update_);
}

AccelerometerFileReader::ReadingData::ReadingData() = default;
AccelerometerFileReader::ReadingData::ReadingData(const ReadingData&) = default;
AccelerometerFileReader::ReadingData::~ReadingData() = default;

AccelerometerFileReader::ConfigurationData::ConfigurationData() : count(0) {
  for (int i = 0; i < ACCELEROMETER_SOURCE_COUNT; ++i) {
    has[i] = false;
    for (int j = 0; j < 3; ++j) {
      scale[i][j] = 0;
      index[i][j] = -1;
    }
  }
}

AccelerometerFileReader::ConfigurationData::~ConfigurationData() = default;

}  // namespace ash
