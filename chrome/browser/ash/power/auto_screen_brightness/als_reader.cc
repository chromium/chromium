// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/auto_screen_brightness/als_reader.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chromeos/components/sensors/buildflags.h"
#if BUILDFLAG(USE_IIOSERVICE)
#include "chrome/browser/ash/power/auto_screen_brightness/light_provider_mojo.h"
#else  // !BUILDFLAG(USE_IIOSERVICE)
#include "chrome/browser/ash/power/auto_screen_brightness/als_file_reader.h"
#endif  // BUILDFLAG(USE_IIOSERVICE)
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

namespace {

#if !BUILDFLAG(USE_IIOSERVICE)
// Returns the number of ALS on this device that we can use. This should run in
// another thread to be non-blocking to the main thread.
int GetNumAls() {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::CommandLine command_line{
      base::FilePath(FILE_PATH_LITERAL("check_powerd_config"))};
  command_line.AppendArg("--ambient_light_sensor");
  int exit_code = 0;
  std::string output = "";
  const bool result =
      base::GetAppOutputWithExitCode(command_line, &output, &exit_code);

  if (!result) {
    LOG(ERROR) << "Cannot run check_powerd_config --ambient_light_sensor";
    return 0;
  }

  base::TrimWhitespaceASCII(output, base::TRIM_ALL, &output);
  int num_als = 0;
  if (exit_code != 0 || output.empty() ||
      !base::StringToInt(output, &num_als)) {
    LOG(ERROR) << "Missing num of als";
    return 0;
  }

  return num_als;
}
#endif  // !BUILDFLAG(USE_IIOSERVICE)

}  // namespace

AlsReader::AlsReader() = default;
AlsReader::~AlsReader() = default;

void AlsReader::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!blocking_task_runner_);

#if BUILDFLAG(USE_IIOSERVICE)
  provider_ = std::make_unique<LightProviderMojo>(this);
#else   // !BUILDFLAG(USE_IIOSERVICE)
  blocking_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetNumAls),
      base::BindOnce(&AlsReader::OnNumAlsRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(USE_IIOSERVICE)
}

void AlsReader::AddObserver(Observer* const observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (status_ != AlsInitStatus::kInProgress)
    observer->OnAlsReaderInitialized(status_);
}

void AlsReader::RemoveObserver(Observer* const observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

#if !BUILDFLAG(USE_IIOSERVICE)
void AlsReader::OnNumAlsRetrieved(int num_als) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (num_als <= 0) {
    SetAlsInitStatus(AlsReader::AlsInitStatus::kDisabled);
    return;
  }

  auto provider = std::make_unique<AlsFileReader>(this);
  provider->Init(std::move(blocking_task_runner_));
  provider_ = std::move(provider);
}
#endif  // !BUILDFLAG(USE_IIOSERVICE)

void AlsReader::SetLux(int lux) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnAmbientLightUpdated(lux);
}

void AlsReader::SetAlsInitStatus(AlsInitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(status, AlsInitStatus::kInProgress);
  status_ = status;
  for (auto& observer : observers_)
    observer.OnAlsReaderInitialized(status_);

  UMA_HISTOGRAM_ENUMERATION("AutoScreenBrightness.AlsReaderStatus", status_);
}

void AlsReader::SetAlsInitStatusForTesting(AlsInitStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  status_ = status;
}

LightProviderInterface::LightProviderInterface(AlsReader* als_reader)
    : als_reader_(als_reader) {}
LightProviderInterface::~LightProviderInterface() = default;

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash
