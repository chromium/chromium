// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_power_controller.h"

#include "ash/shell.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "content/public/browser/device_service.h"
#include "ui/views/widget/widget.h"

namespace borealis {

constexpr char kBorealisVmName[] = "borealis";

BorealisPowerController::BorealisPowerController(Profile* profile)
    : owner_id_(ash::ProfileHelper::GetUserIdHashFromProfile(profile)) {
  ash::CiceroneClient::Get()->AddObserver(this);
}

void BorealisPowerController::EnsureWakeLock() {
  if (!wake_lock_provider_)
    content::GetDeviceService().BindWakeLockProvider(
        wake_lock_provider_.BindNewPipeAndPassReceiver());
  if (!wake_lock_)
    wake_lock_provider_->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        device::mojom::WakeLockReason::kOther, /*description=*/"Borealis",
        wake_lock_.BindNewPipeAndPassReceiver());
  wake_lock_->RequestWakeLock();
}

BorealisPowerController::~BorealisPowerController() {
  if (wake_lock_) {
    wake_lock_->CancelWakeLock();
  }
  ash::CiceroneClient::Get()->RemoveObserver(this);
}

void BorealisPowerController::OnInhibitScreensaver(
    const vm_tools::cicerone::InhibitScreensaverSignal& signal) {
  if (signal.vm_name() != kBorealisVmName || owner_id_ != signal.owner_id()) {
    return;
  }
  EnsureWakeLock();
  // There is currently no wake lock.
  if (cookies_.empty()) {
    EnsureWakeLock();
  }
  cookies_.insert(signal.cookie());
}

void BorealisPowerController::OnUninhibitScreensaver(
    const vm_tools::cicerone::UninhibitScreensaverSignal& signal) {
  if (signal.vm_name() != kBorealisVmName || owner_id_ != signal.owner_id()) {
    return;
  }
  cookies_.erase(signal.cookie());
  // There are no inhibit messages that have not been uninhibited.
  if (cookies_.empty() && wake_lock_) {
    wake_lock_->CancelWakeLock();
  }
}
}  // namespace borealis
