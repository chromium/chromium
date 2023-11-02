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
#include "ui/aura/client/focus_client.h"
#include "ui/views/widget/widget.h"

namespace borealis {

constexpr char kBorealisVmName[] = "borealis";
// Real cookies are uint32 so -1 will not conflict with a real cookie.
int kFakeCookieForFocusInhibit = -1;

BorealisPowerController::BorealisPowerController(Profile* profile)
    : profile_(profile),
      owner_id_(ash::ProfileHelper::GetUserIdHashFromProfile(profile)) {
  if (ash::Shell::HasInstance())
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
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
  if (ash::Shell::HasInstance())
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
  ash::CiceroneClient::Get()->RemoveObserver(this);
}

void BorealisPowerController::OnWindowFocused(aura::Window* gained_focus,
                                              aura::Window* lost_focus) {
  bool should_wake_lock = false;
  if (gained_focus) {
    auto* widget = views::Widget::GetTopLevelWidgetForNativeView(gained_focus);
    aura::Window* window = widget->GetNativeWindow();
    if (BorealisService::GetForProfile(profile_)->WindowManager().GetShelfAppId(
            window) == kClientAppId) {
      should_wake_lock = true;
    }
  }
  // Send synthetic inhibit/uninhibit messages. The cookie 0 is not used by
  // the actual D-Bus server.
  if (!should_wake_lock) {
    vm_tools::cicerone::UninhibitScreensaverSignal uninhibit;
    uninhibit.set_vm_name(kBorealisVmName);
    uninhibit.set_owner_id(owner_id_);
    uninhibit.set_cookie(kFakeCookieForFocusInhibit);
    OnUninhibitScreensaver(uninhibit);
    return;
  }
  vm_tools::cicerone::InhibitScreensaverSignal inhibit;
  inhibit.set_vm_name(kBorealisVmName);
  inhibit.set_owner_id(owner_id_);
  inhibit.set_cookie(kFakeCookieForFocusInhibit);
  OnInhibitScreensaver(inhibit);
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
