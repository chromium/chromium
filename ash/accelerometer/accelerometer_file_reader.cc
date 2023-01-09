// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_file_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "ash/accelerometer/accelerometer_constants.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/numerics/math_constants.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"

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

// The axes on each accelerometer. The order was changed on kernel 3.18+.
constexpr char kAccelerometerAxes[][2] = {"x", "y", "z"};
constexpr char kLegacyAccelerometerAxes[][2] = {"y", "x", "z"};

// The length required to read uint values from configuration files.
constexpr size_t kMaxAsciiUintLength = 21;

// The size of individual values.
constexpr size_t kDataSize = 2;

// The size of data in one reading of the accelerometers.
constexpr int kSizeOfReading = kDataSize * kNumberOfAxes;

// The time to wait between reading the accelerometer.
constexpr base::TimeDelta kDelayBetweenReads = base::Milliseconds(100);

// The TimeDelta before giving up on initialization. This is needed because the
// sensor hub might not be online when the Initialize function is called.
constexpr base::TimeDelta kInitializeTimeout = base::Seconds(5);

// The time between initialization checks.
constexpr base::TimeDelta kDelayBetweenInitChecks = base::Milliseconds(500);

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

AccelerometerFileReader::AccelerometerFileReader() = default;

void AccelerometerFileReader::PrepareAndInitialize() {
  DCHECK(base::CurrentUIThread::IsSet());

  DETACH_FROM_SEQUENCE(sequence_checker_);

  // AccelerometerReader is important for screen orientation so we need
  // USER_VISIBLE priority.
  // Use CONTINUE_ON_SHUTDOWN to avoid blocking shutdown since the datareading
  // could get blocked on certain devices. See https://crbug.com/1023989.
  blocking_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  initialization_state_ = State::INITIALIZING;

  initialization_timeout_ = base::TimeTicks::Now() + kInitializeTimeout;

  TryScheduleInitialize();
}

void AccelerometerFileReader::TriggerRead() {
  DCHECK(base::CurrentUIThread::IsSet());
  switch (initialization_state_) {
    case State::SUCCESS:
      if (GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::SUPPORTED) {
        blocking_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&AccelerometerFileReader::EnableAccelerometerReading,
                           this));
      }
      break;
    case State::FAILED:
      LOG(ERROR) << "Failed to initialize for accelerometer read.\n";
      break;
    case State::INITIALIZING:
      ui_task_runner_->PostNonNestableDelayedTask(
          FROM_HERE,
          base::BindOnce(&AccelerometerFileReader::TriggerRead, this),
          kDelayBetweenInitChecks);
      break;
  }
}

void AccelerometerFileReader::CancelRead() {
  DCHECK(base::CurrentUIThread::IsSet());
  if (initialization_state_ == State::SUCCESS &&
      GetECLidAngleDriverStatus() == ECLidAngleDriverStatus::SUPPORTED) {
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AccelerometerFileReader::DisableAccelerometerReading,
                       this));
  }
}

AccelerometerFileReader::InitializationResult::InitializationResult()
    : initialization_state(State::INITIALIZING),
      ec_lid_angle_driver_status(ECLidAngleDriverStatus::UNKNOWN) {}
AccelerometerFileReader::InitializationResult::~InitializationResult() =
    default;

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

AccelerometerFileReader::~AccelerometerFileReader() = default;

void AccelerometerFileReader::TryScheduleInitialize() {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(blocking_task_runner_);

  // Asynchronously detect and initialize the accelerometer to avoid delaying
  // startup.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AccelerometerFileReader::InitializeInternal, this),
      base::BindOnce(
          &AccelerometerFileReader::SetStatesWithInitializationResult, this));
}

AccelerometerFileReader::InitializationResult
AccelerometerFileReader::InitializeInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Log the warning/error messages only in the first initialization to prevent
  // spamming during the retries of initialization.
  static bool first_initialization_ = true;

  InitializationResult result;

  // Check for accelerometer symlink which will be created by the udev rules
  // file on detecting the device.
  if (base::IsDirectoryEmpty(base::FilePath(kAccelerometerDevicePath))) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      if (first_initialization_) {
        LOG(WARNING) << "Accelerometer device directory is empty at "
                     << kAccelerometerDevicePath;
      }
      first_initialization_ = false;
      return result;
    }

    result.initialization_state = State::FAILED;
    return result;
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
        result.initialization_state = State::FAILED;
        return result;
      } else {
        configuration_.trigger_now = trigger_now;
        break;
      }
    }
  }
  if (configuration_.trigger_now.empty()) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      if (first_initialization_)
        LOG(ERROR) << "Accelerometer trigger not found";
      first_initialization_ = false;
      return result;
    }

    result.initialization_state = State::FAILED;
    return result;
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
      result.initialization_state = State::FAILED;
      return result;
    }

    base::FilePath iio_path(base::FilePath(kAccelerometerIioBasePath)
                                .Append(iio_device.BaseName()));
    std::string location;
    legacy_cross_accel = !base::ReadFileToString(
        base::FilePath(iio_path).Append(kAccelerometerLocationFileName),
        &location);
    if (legacy_cross_accel) {
      if (!InitializeLegacyAccelerometers(iio_path, name)) {
        result.initialization_state = State::FAILED;
        return result;
      }
    } else {
      base::TrimWhitespaceASCII(location, base::TRIM_ALL, &location);
      if (!InitializeAccelerometer(iio_path, name, location)) {
        result.initialization_state = State::FAILED;
        return result;
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
        LOG(ERROR) << "Field index for " << kLocationStrings[i] << " " << axis
                   << " axis out of bounds.";
        result.initialization_state = State::FAILED;
        return result;
      }
    }
  }

  result.initialization_state = State::SUCCESS;
  result.ec_lid_angle_driver_status =
      (base::SysInfo::IsRunningOnChromeOS() &&
       !base::IsDirectoryEmpty(base::FilePath(kECLidAngleDriverPath)))
          ? ECLidAngleDriverStatus::SUPPORTED
          : ECLidAngleDriverStatus::NOT_SUPPORTED;

  return result;
}

void AccelerometerFileReader::SetStatesWithInitializationResult(
    InitializationResult result) {
  DCHECK(base::CurrentUIThread::IsSet());

  initialization_state_ = result.initialization_state;
  switch (initialization_state_) {
    case State::INITIALIZING:
      // If we haven't yet passed the timeout cutoff, try this again. This will
      // be scheduled at the same rate as reading.
      if (base::TimeTicks::Now() < initialization_timeout_) {
        ui_task_runner_->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&AccelerometerFileReader::TryScheduleInitialize,
                           this),
            kDelayBetweenReads);
      } else {
        LOG(ERROR) << "Failed to initialize for accelerometer read.\n";
        initialization_state_ = State::FAILED;
      }

      break;

    case State::SUCCESS:
      DCHECK_NE(result.ec_lid_angle_driver_status,
                ECLidAngleDriverStatus::UNKNOWN);
      SetECLidAngleDriverStatus(result.ec_lid_angle_driver_status);

      if (GetECLidAngleDriverStatus() ==
          ECLidAngleDriverStatus::NOT_SUPPORTED) {
        // If ChromeOS lid angle driver is not present, start accelerometer read
        // and read is always on.
        blocking_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&AccelerometerFileReader::EnableAccelerometerReading,
                           this));
      }

      break;

    case State::FAILED:
      break;

    default:
      LOG(FATAL) << "Unexpected state: "
                 << static_cast<int>(initialization_state_);
      break;
  }
}

bool AccelerometerFileReader::InitializeAccelerometer(
    const base::FilePath& iio_path,
    const base::FilePath& name,
    const std::string& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t config_index = 0;
  for (; config_index < std::size(kLocationStrings); ++config_index) {
    if (location == kLocationStrings[config_index])
      break;
  }

  if (config_index >= std::size(kLocationStrings)) {
    LOG(ERROR) << "Unrecognized location: " << location << " for device "
               << name.MaybeAsASCII() << "\n";
    return false;
  }

  double scale;
  if (!ReadFileToDouble(iio_path.Append(kAccelerometerScaleFileName), &scale))
    return false;

  const int kNumberAxes = std::size(kAccelerometerAxes);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReadingData reading_data;
  reading_data.path =
      base::FilePath(kAccelerometerDevicePath).Append(name.BaseName());
  // Read configuration of each accelerometer axis from each accelerometer from
  // /sys/bus/iio/devices/iio:deviceX/.
  for (size_t i = 0; i < std::size(kLocationStrings); ++i) {
    configuration_.has[i] = false;
    // Read scale of accelerometer.
    std::string accelerometer_scale_path =
        base::StringPrintf(kLegacyScaleNameFormatString, kLocationStrings[i]);
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
    for (size_t j = 0; j < std::size(kLegacyAccelerometerAxes); ++j) {
      configuration_.scale[i][j] = base::kMeanGravityFloat / scale_divisor;
      std::string accelerometer_index_path =
          base::StringPrintf(kLegacyAccelerometerScanIndexPathFormatString,
                             kLegacyAccelerometerAxes[j], kLocationStrings[i]);
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

void AccelerometerFileReader::EnableAccelerometerReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (read_refresh_timer_.IsRunning())
    return;

  read_refresh_timer_.Start(FROM_HERE, kDelayBetweenReads, this,
                            &AccelerometerFileReader::ReadSample);
}

void AccelerometerFileReader::DisableAccelerometerReading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  read_refresh_timer_.Stop();
}

void AccelerometerFileReader::ReadSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Initiate the trigger to read accelerometers simultaneously.
  int bytes_written = base::WriteFile(configuration_.trigger_now, "1\n", 2);
  if (bytes_written < 2) {
    PLOG(ERROR) << "Accelerometer trigger failure: " << bytes_written;
    return;
  }

  // Read resulting sample from /dev/cros-ec-accel.
  AccelerometerUpdate update;
  for (auto reading_data : configuration_.reading_data) {
    int reading_size = reading_data.sources.size() * kSizeOfReading;
    DCHECK_GT(reading_size, 0);
    char reading[reading_size];
    int bytes_read = base::ReadFile(reading_data.path, reading, reading_size);
    if (bytes_read < reading_size) {
      // Dynamically throttle error logging as this can be called many times
      // every second if path is consistently inaccessible.
      static uint64_t sLogCount = 1U;
      static uint64_t sLogThrottle = 1U;
      if ((sLogCount ^ sLogThrottle) == 0) {
        LOG(ERROR) << "Accelerometer Read " << bytes_read
                   << " byte(s), expected " << reading_size
                   << " bytes from accelerometer "
                   << reading_data.path.MaybeAsASCII();
        sLogThrottle *= 2U;
      }
      sLogCount++;
      return;
    }
    for (AccelerometerSource source : reading_data.sources) {
      DCHECK(configuration_.has[source]);
      int16_t* values = reinterpret_cast<int16_t*>(reading);
      update.Set(source,
                 values[configuration_.index[source][0]] *
                     configuration_.scale[source][0],
                 values[configuration_.index[source][1]] *
                     configuration_.scale[source][1],
                 values[configuration_.index[source][2]] *
                     configuration_.scale[source][2]);
    }
  }

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AccelerometerFileReader::NotifyAccelerometerUpdated, this,
                     update));
}

}  // namespace ash
