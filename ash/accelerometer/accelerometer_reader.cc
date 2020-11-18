// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_reader.h"

#include <grp.h>

#include "ash/accelerometer/accelerometer_file_reader.h"
#include "ash/accelerometer/accelerometer_provider_mojo.h"
#include "base/no_destructor.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace ash {

namespace {

// Group name of IIO Service, used to check if IIO Service exists.
constexpr char kIioServiceGroupName[] = "iioservice";

}  // namespace

// static
AccelerometerReader* AccelerometerReader::GetInstance() {
  static base::NoDestructor<AccelerometerReader> accelerometer_reader;
  return accelerometer_reader.get();
}

void AccelerometerReader::Initialize() {
  accelerometer_provider_->PrepareAndInitialize();
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

ECLidAngleDriverStatus AccelerometerReader::GetECLidAngleDriverStatus() const {
  return accelerometer_provider_->GetECLidAngleDriverStatus();
}

void AccelerometerReader::SetECLidAngleDriverStatusForTesting(
    ECLidAngleDriverStatus ec_lid_angle_driver_status) {
  accelerometer_provider_->SetECLidAngleDriverStatusForTesting(  // IN-TEST
      ec_lid_angle_driver_status);
}

AccelerometerReader::AccelerometerReader() {
  char buf[1024];
  struct group result;
  struct group* resultp;

  if (HANDLE_EINTR(getgrnam_r(kIioServiceGroupName, &result, buf, sizeof(buf),
                              &resultp)) < 0 ||
      !resultp) {
    accelerometer_provider_ = new AccelerometerFileReader();
  } else {
    accelerometer_provider_ = new AccelerometerProviderMojo();
  }
}

AccelerometerReader::~AccelerometerReader() = default;

ECLidAngleDriverStatus
AccelerometerProviderInterface::GetECLidAngleDriverStatus() const {
  return ec_lid_angle_driver_status_;
}

void AccelerometerProviderInterface::SetECLidAngleDriverStatusForTesting(
    ECLidAngleDriverStatus ec_lid_angle_driver_status) {
  ec_lid_angle_driver_status_ = ec_lid_angle_driver_status;
}

}  // namespace ash
