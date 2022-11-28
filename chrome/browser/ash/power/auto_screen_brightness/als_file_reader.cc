// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/als_file_reader.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/power/auto_screen_brightness/utils.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {
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

constexpr base::TimeDelta AlsFileReader::kAlsFileCheckingInterval;
constexpr int AlsFileReader::kMaxInitialAttempts;
constexpr base::TimeDelta AlsFileReader::kAlsPollInterval;

AlsFileReader::AlsFileReader(AlsReader* als_reader)
    : LightProviderInterface(als_reader) {}

AlsFileReader::~AlsFileReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AlsFileReader::Init(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(als_reader_);
  blocking_task_runner_ = blocking_task_runner;
  RetryAlsPath();
}

void AlsFileReader::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner> test_blocking_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_ = test_blocking_task_runner;
}

void AlsFileReader::InitForTesting(const base::FilePath& ambient_light_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ambient_light_path.empty());
  DCHECK(als_reader_);
  ambient_light_path_ = ambient_light_path;
  als_reader_->SetAlsInitStatus(AlsReader::AlsInitStatus::kSuccess);
  ReadAlsPeriodically();
}

void AlsFileReader::FailForTesting() {
  als_reader_->SetAlsInitStatus(AlsReader::AlsInitStatus::kDisabled);
  for (int i = 0; i <= kMaxInitialAttempts; i++)
    OnAlsPathReadAttempted("");
}

void AlsFileReader::OnAlsPathReadAttempted(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(als_reader_);
  if (!path.empty()) {
    ambient_light_path_ = base::FilePath(path);
    als_reader_->SetAlsInitStatus(AlsReader::AlsInitStatus::kSuccess);
    ReadAlsPeriodically();
    return;
  }

  ++num_failed_initialization_;

  if (num_failed_initialization_ == kMaxInitialAttempts) {
    als_reader_->SetAlsInitStatus(AlsReader::AlsInitStatus::kMissingPath);
    return;
  }

  als_timer_.Start(FROM_HERE, kAlsFileCheckingInterval, this,
                   &AlsFileReader::RetryAlsPath);
}

void AlsFileReader::RetryAlsPath() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetAlsPath),
      base::BindOnce(&AlsFileReader::OnAlsPathReadAttempted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsFileReader::ReadAlsPeriodically() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadAlsFromFile, ambient_light_path_),
      base::BindOnce(&AlsFileReader::OnAlsRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlsFileReader::OnAlsRead(const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(als_reader_);
  std::string trimmed_data;
  base::TrimWhitespaceASCII(data, base::TRIM_ALL, &trimmed_data);
  int value = 0;
  if (base::StringToInt(trimmed_data, &value)) {
    if (value < 0) {
      LogDataError(DataError::kAlsValue);
      value = 0;
    }

    als_reader_->SetLux(value);
  }
  als_timer_.Start(FROM_HERE, kAlsPollInterval, this,
                   &AlsFileReader::ReadAlsPeriodically);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
