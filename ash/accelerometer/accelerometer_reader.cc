// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_reader.h"

#include "ash/accelerometer/accelerometer_file_reader.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"

namespace ash {

// static
AccelerometerReader* AccelerometerReader::GetInstance() {
  return base::Singleton<AccelerometerReader>::get();
}

void AccelerometerReader::Initialize(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner) {
  DCHECK(sequenced_task_runner.get());

  accelerometer_provider_->PrepareAndInitialize(sequenced_task_runner);
}

void AccelerometerReader::AddObserver(Observer* observer) {
  accelerometer_provider_->AddObserver(observer);
}

void AccelerometerReader::RemoveObserver(Observer* observer) {
  accelerometer_provider_->RemoveObserver(observer);
}

void AccelerometerReader::StartListenToTabletModeController() {
  accelerometer_provider_->StartListenToTabletModeController();
}

void AccelerometerReader::StopListenToTabletModeController() {
  accelerometer_provider_->StopListenToTabletModeController();
}

void AccelerometerReader::SetEnabled(bool enabled) {
  accelerometer_provider_->SetEmitEvents(enabled);
}

AccelerometerReader::AccelerometerReader()
    : accelerometer_provider_(new AccelerometerFileReader()) {}

AccelerometerReader::~AccelerometerReader() = default;

}  // namespace ash
