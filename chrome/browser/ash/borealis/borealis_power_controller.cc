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
constexpr char kDownloadInhibitReason[] = "download";
constexpr char kWakelockReason[] = "Borealis";
constexpr char kWakelockReasonDownlaod[] = "Borealis game download";

BorealisPowerController::BorealisPowerController(Profile* profile)
    : owner_id_(ash::ProfileHelper::GetUserIdHashFromProfile(profile)) {
  ash::CiceroneClient::Get()->AddObserver(this);
}

BorealisPowerController::~BorealisPowerController() {
  if (wake_lock_) {
    wake_lock_->CancelWakeLock();
  }
  if (download_wake_lock_) {
    download_wake_lock_->CancelWakeLock();
  }
  ash::CiceroneClient::Get()->RemoveObserver(this);
}

void BorealisPowerController::OnInhibitScreensaver(
    const vm_tools::cicerone::InhibitScreensaverSignal& signal) {
  if (signal.vm_name() != kBorealisVmName || owner_id_ != signal.owner_id()) {
    return;
  }
  if (!wake_lock_provider_)
    content::GetDeviceService().BindWakeLockProvider(
        wake_lock_provider_.BindNewPipeAndPassReceiver());

  if (signal.reason() == kDownloadInhibitReason) {
    // Currently no inhibit active.
    if (download_cookies_.empty()) {
      if (!download_wake_lock_) {
        wake_lock_provider_->GetWakeLockWithoutContext(
            device::mojom::WakeLockType::kPreventAppSuspension,
            device::mojom::WakeLockReason::kOther, kWakelockReasonDownlaod,
            download_wake_lock_.BindNewPipeAndPassReceiver());
      }
      download_wake_lock_->RequestWakeLock();
    }
    download_cookies_.insert(signal.cookie());
  } else {
    if (cookies_.empty()) {
      if (!wake_lock_) {
        wake_lock_provider_->GetWakeLockWithoutContext(
            device::mojom::WakeLockType::kPreventDisplaySleep,
            device::mojom::WakeLockReason::kOther, kWakelockReason,
            wake_lock_.BindNewPipeAndPassReceiver());
      }
      wake_lock_->RequestWakeLock();
    }
    cookies_.insert(signal.cookie());
  }
}

void BorealisPowerController::OnUninhibitScreensaver(
    const vm_tools::cicerone::UninhibitScreensaverSignal& signal) {
  if (signal.vm_name() != kBorealisVmName || owner_id_ != signal.owner_id()) {
    return;
  }
  if (download_cookies_.erase(signal.cookie())) {
    // There are no inhibit messages that have not been uninhibited.
    if (download_cookies_.empty() && download_wake_lock_)
      download_wake_lock_->CancelWakeLock();
  } else if (cookies_.erase(signal.cookie())) {
    if (cookies_.empty() && wake_lock_)
      wake_lock_->CancelWakeLock();
  } else {
    LOG(ERROR) << "Invalid uninhibit cookie: " << signal.cookie();
  }
}
}  // namespace borealis
