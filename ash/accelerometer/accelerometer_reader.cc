// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerometer/accelerometer_reader.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "ash/accelerometer/accelerometer_provider_mojo.h"

namespace ash {

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

void AccelerometerReader::SetECLidAngleDriverStatusForTesting(
    ECLidAngleDriverStatus ec_lid_angle_driver_status) {
  accelerometer_provider_->SetECLidAngleDriverStatusForTesting(  // IN-TEST
      ec_lid_angle_driver_status);
}

AccelerometerReader::AccelerometerReader() {
  accelerometer_provider_ = new AccelerometerProviderMojo();
}

AccelerometerReader::~AccelerometerReader() = default;

void AccelerometerProviderInterface::OnTabletPhysicalStateChanged() {
  DCHECK(base::CurrentUIThread::IsSet());

  if (ShouldDelayOnTabletPhysicalStateChanged())
    return;

  // When CrOS EC lid angle driver is not present, accelerometer read is always
  // ON and can't be tuned. Thus this object no longer listens to tablet mode
  // event.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  if (ec_lid_angle_driver_status_ == ECLidAngleDriverStatus::NOT_SUPPORTED) {
    tablet_mode_controller->RemoveObserver(this);
    return;
  }

  // Auto rotation is turned on when the device is physically used as a tablet
  // (i.e. flipped or detached), regardless of the UI state (i.e. whether tablet
  // mode is turned on or off).
  if (tablet_mode_controller->is_in_tablet_physical_state())
    TriggerRead();
  else
    CancelRead();
}

void AccelerometerProviderInterface::AddObserver(
    AccelerometerReader::Observer* observer) {
  DCHECK(base::CurrentUIThread::IsSet());

  if (ec_lid_angle_driver_status_ != ECLidAngleDriverStatus::UNKNOWN) {
    observer->OnECLidAngleDriverStatusChanged(
        ec_lid_angle_driver_status_ == ECLidAngleDriverStatus::SUPPORTED);
  }

  observers_.AddObserver(observer);
}

void AccelerometerProviderInterface::RemoveObserver(
    AccelerometerReader::Observer* observer) {
  DCHECK(base::CurrentUIThread::IsSet());
  observers_.RemoveObserver(observer);
}

void AccelerometerProviderInterface::StartListenToTabletModeController() {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

void AccelerometerProviderInterface::StopListenToTabletModeController() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void AccelerometerProviderInterface::SetEmitEvents(bool emit_events) {
  DCHECK(base::CurrentUIThread::IsSet());
  emit_events_ = emit_events;
}

void AccelerometerProviderInterface::SetECLidAngleDriverStatusForTesting(
    ECLidAngleDriverStatus status) {
  SetECLidAngleDriverStatus(status);
}

AccelerometerProviderInterface::AccelerometerProviderInterface()
    : ui_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(base::CurrentUIThread::IsSet());
}

AccelerometerProviderInterface::~AccelerometerProviderInterface() = default;

bool AccelerometerProviderInterface::ShouldDelayOnTabletPhysicalStateChanged() {
  return false;
}

void AccelerometerProviderInterface::SetECLidAngleDriverStatus(
    ECLidAngleDriverStatus status) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK_NE(status, ECLidAngleDriverStatus::UNKNOWN);

  if (status == ec_lid_angle_driver_status_)
    return;

  ec_lid_angle_driver_status_ = status;

  for (auto& observer : observers_) {
    observer.OnECLidAngleDriverStatusChanged(ec_lid_angle_driver_status_ ==
                                             ECLidAngleDriverStatus::SUPPORTED);
  }
}

ECLidAngleDriverStatus
AccelerometerProviderInterface::GetECLidAngleDriverStatus() const {
  DCHECK(base::CurrentUIThread::IsSet());

  return ec_lid_angle_driver_status_;
}

void AccelerometerProviderInterface::NotifyAccelerometerUpdated(
    const AccelerometerUpdate& update) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK_NE(ec_lid_angle_driver_status_, ECLidAngleDriverStatus::UNKNOWN);

  if (!emit_events_)
    return;

  for (auto& observer : observers_)
    observer.OnAccelerometerUpdated(update);
}

}  // namespace ash
