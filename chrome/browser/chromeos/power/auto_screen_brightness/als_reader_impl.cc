// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/als_reader_impl.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
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

// Returns whether the ALS step config is what we need. This function is only
// called if an ALS is enabled. This should run in another thread to be
// non-blocking to the main thread.
// TODO(jiameng): we assume one specific device now, and only check if the
// number of steps is 7.
bool VerifyAlsConfig() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::CommandLine command_line{
      base::FilePath(FILE_PATH_LITERAL("check_powerd_config"))};
  command_line.AppendArg("--internal_backlight_ambient_light_steps");
  int exit_code = 0;
  std::string output;
  const bool result =
      base::GetAppOutputWithExitCode(command_line, &output, &exit_code);

  if (!result || exit_code != 0) {
    LOG(ERROR) << "Cannot run check_powerd_config "
                  "--internal_backlight_ambient_light_steps";
    return false;
  }

  const std::vector<base::StringPiece> num_steps = base::SplitStringPiece(
      output, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return num_steps.size() == 7;
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
    : als_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      weak_ptr_factory_(this) {}

AlsReaderImpl::~AlsReaderImpl() = default;

void AlsReaderImpl::AddObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (status_ != AlsInitStatus::kInProgress)
    observer->OnAlsReaderInitialized(status_);
}

void AlsReaderImpl::RemoveObserver(Observer* const observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void AlsReaderImpl::Init() {
  base::PostTaskAndReplyWithResult(
      als_task_runner_.get(), FROM_HERE, base::BindOnce(&IsAlsEnabled),
      base::BindOnce(&AlsReaderImpl::OnAlsEnableCheckDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsReaderImpl::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner> task_runner) {
  als_task_runner_ = task_runner;
  als_timer_.SetTaskRunner(task_runner);
}

void AlsReaderImpl::InitForTesting(const base::FilePath& ambient_light_path) {
  DCHECK(!ambient_light_path.empty());
  ambient_light_path_ = ambient_light_path;
  status_ = AlsInitStatus::kSuccess;
  OnInitializationComplete();
  ReadAlsPeriodically();
}

void AlsReaderImpl::OnAlsEnableCheckDone(const bool is_enabled) {
  if (!is_enabled) {
    status_ = AlsInitStatus::kDisabled;
    OnInitializationComplete();
    return;
  }

  base::PostTaskAndReplyWithResult(
      als_task_runner_.get(), FROM_HERE, base::BindOnce(&VerifyAlsConfig),
      base::BindOnce(&AlsReaderImpl::OnAlsConfigCheckDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsReaderImpl::OnAlsConfigCheckDone(const bool is_config_valid) {
  if (!is_config_valid) {
    status_ = AlsInitStatus::kIncorrectConfig;
    OnInitializationComplete();
    return;
  }

  RetryAlsPath();
}

void AlsReaderImpl::OnAlsPathReadAttempted(const std::string& path) {
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
  base::PostTaskAndReplyWithResult(
      als_task_runner_.get(), FROM_HERE, base::BindOnce(&GetAlsPath),
      base::BindOnce(&AlsReaderImpl::OnAlsPathReadAttempted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsReaderImpl::OnInitializationComplete() {
  DCHECK_NE(status_, AlsInitStatus::kInProgress);
  for (auto& observer : observers_)
    observer.OnAlsReaderInitialized(status_);
}

void AlsReaderImpl::ReadAlsPeriodically() {
  base::PostTaskAndReplyWithResult(
      als_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ReadAlsFromFile, ambient_light_path_),
      base::BindOnce(&AlsReaderImpl::OnAlsRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsReaderImpl::OnAlsRead(const std::string& data) {
  std::string trimmed_data;
  base::TrimWhitespaceASCII(data, base::TRIM_ALL, &trimmed_data);
  int value = 0;
  if (base::StringToInt(trimmed_data, &value)) {
    for (auto& observer : observers_)
      observer.OnAmbientLightUpdated(value);
  }
  als_timer_.Start(FROM_HERE, kAlsPollInterval, this,
                   &AlsReaderImpl::ReadAlsPeriodically);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
