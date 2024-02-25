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
#include "chrome/browser/ash/power/auto_screen_brightness/light_provider_mojo.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

AlsReader::AlsReader() = default;
AlsReader::~AlsReader() = default;

void AlsReader::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!blocking_task_runner_);

  provider_ = std::make_unique<LightProviderMojo>(this);
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
