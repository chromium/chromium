// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {
// Returns whether the device has an ALS that we can use. This should run in
// another thread to be non-blocking to the main thread.
bool IsAlsEnabled() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::CommandLine command_line{
      base::FilePath(FILE_PATH_LITERAL("check_powerd_config"))};
  command_line.AppendArg("--ambient_light_sensor");
  int exit_code = 0;
  std::string output;  // Not used.
  const bool result =
      base::GetAppOutputWithExitCode(command_line, &output, &exit_code);

  if (!result) {
    LOG(ERROR) << "Cannot run check_powerd_config --ambient_light_sensor";
    return false;
  }
  return exit_code == 0;
}

// Returns ALS path. This should run in another thread to be non-blocking to the
// main thread.
std::string GetAlsPath() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::CommandLine command_line{
      base::FilePath(FILE_PATH_LITERAL("backlight_tool"))};
  command_line.AppendArg("--get_ambient_light_path");
  int exit_code = 0;
  std::string output;
  const bool result =
      base::GetAppOutputWithExitCode(command_line, &output, &exit_code);
  if (!result) {
    LOG(ERROR) << "Cannot run backlight_tool --get_ambient_light_path";
    return "";
  }

  base::TrimWhitespaceASCII(output, base::TRIM_ALL, &output);

  if (exit_code != 0 || output.empty()) {
    LOG(ERROR) << "Missing ambient light path";
    return "";
  }

  return output;
}

// Reads ALS value from |ambient_light_path|. This should run in another thread
// to be non-blocking to the main thread.
std::string ReadAlsFromFile(const base::FilePath& ambient_light_path) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string data;
  if (!base::ReadFileToString(ambient_light_path, &data)) {
    LOG(ERROR) << "Cannot read ALS value";
    return "";
  }
  return data;
}
}  // namespace

constexpr base::TimeDelta AlsReaderImpl::kAlsFileCheckingInterval;
constexpr int AlsReaderImpl::kMaxInitialAttempts;
constexpr base::TimeDelta AlsReaderImpl::kAlsPollInterval;

AlsReaderImpl::AlsReaderImpl()
    : blocking_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
           base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

AlsReaderImpl::~AlsReaderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AlsReaderImpl::AddObserver(Observer* const observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (status_ != AlsInitStatus::kInProgress)
    observer->OnAlsReaderInitialized(status_);
}

void AlsReaderImpl::RemoveObserver(Observer* const observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void AlsReaderImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE, base::BindOnce(&IsAlsEnabled),
      base::BindOnce(&AlsReaderImpl::OnAlsEnableCheckDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsReaderImpl::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner> test_blocking_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_ = test_blocking_task_runner;
}

void AlsReaderImpl::InitForTesting(const base::FilePath& ambient_light_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ambient_light_path.empty());
  ambient_light_path_ = ambient_light_path;
  status_ = AlsInitStatus::kSuccess;
  OnInitializationComplete();
  ReadAlsPeriodically();
}

void AlsReaderImpl::FailForTesting() {
  OnAlsEnableCheckDone(false);
  for (int i = 0; i <= kMaxInitialAttempts; i++)
    OnAlsPathReadAttempted("");
}

void AlsReaderImpl::OnAlsEnableCheckDone(const bool is_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_enabled) {
    status_ = AlsInitStatus::kDisabled;
    OnInitializationComplete();
    return;
  }

  RetryAlsPath();
}

void AlsReaderImpl::OnAlsPathReadAttempted(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!path.empty()) {
    ambient_light_path_ = base::FilePath(path);
    status_ = AlsInitStatus::kSuccess;
    OnInitializationComplete();
    ReadAlsPeriodically();
    return;
  }

  ++num_failed_initialization_;

  if (num_failed_initialization_ == kMaxInitialAttempts) {
    status_ = AlsInitStatus::kMissingPath;
    OnInitializationComplete();
    return;
  }

  als_timer_.Start(FROM_HERE, kAlsFileCheckingInterval, this,
                   &AlsReaderImpl::RetryAlsPath);
}

void AlsReaderImpl::RetryAlsPath() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE, base::BindOnce(&GetAlsPath),
      base::BindOnce(&AlsReaderImpl::OnAlsPathReadAttempted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsReaderImpl::OnInitializationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(status_, AlsInitStatus::kInProgress);
  for (auto& observer : observers_)
    observer.OnAlsReaderInitialized(status_);
  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.AlsReaderStatus", status_);
}

void AlsReaderImpl::ReadAlsPeriodically() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ReadAlsFromFile, ambient_light_path_),
      base::BindOnce(&AlsReaderImpl::OnAlsRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsReaderImpl::OnAlsRead(const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string trimmed_data;
  base::TrimWhitespaceASCII(data, base::TRIM_ALL, &trimmed_data);
  int value = 0;
  if (base::StringToInt(trimmed_data, &value)) {
    if (value < 0) {
      LogDataError(DataError::kAlsValue);
      value = 0;
    }
    for (auto& observer : observers_)
      observer.OnAmbientLightUpdated(value);
  }
  als_timer_.Start(FROM_HERE, kAlsPollInterval, this,
                   &AlsReaderImpl::ReadAlsPeriodically);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
