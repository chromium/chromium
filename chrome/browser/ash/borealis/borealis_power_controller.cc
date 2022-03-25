// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_power_controller.h"

#include "ash/shell.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "content/public/browser/device_service.h"
#include "ui/aura/client/focus_client.h"
#include "ui/views/widget/widget.h"

namespace borealis {

BorealisPowerController::BorealisPowerController() {
  if (ash::Shell::HasInstance())
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
}

BorealisPowerController::~BorealisPowerController() {
  if (wake_lock_) {
    wake_lock_->CancelWakeLock();
  }
  if (ash::Shell::HasInstance())
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
}

void BorealisPowerController::OnWindowFocused(aura::Window* gained_focus,
                                              aura::Window* lost_focus) {
  bool should_wake_lock = false;
  if (gained_focus) {
    auto* widget = views::Widget::GetTopLevelWidgetForNativeView(gained_focus);
    aura::Window* window = widget->GetNativeWindow();
    if (BorealisWindowManager::IsBorealisWindow(window)) {
      should_wake_lock = true;
    }
  }
  if (!should_wake_lock) {
    if (wake_lock_) {
      wake_lock_->CancelWakeLock();
    }
    return;
  }
  // Initialize |wake_lock_| if this is the first time we're using it.
  if (!wake_lock_) {
    // Initialize |wake_lock_provider_| if we haven't already.
    if (!wake_lock_provider_) {
      content::GetDeviceService().BindWakeLockProvider(
          wake_lock_provider_.BindNewPipeAndPassReceiver());
    }
    wake_lock_provider_->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventDisplaySleep,
        device::mojom::WakeLockReason::kOther, "Borealis",
        wake_lock_.BindNewPipeAndPassReceiver());
  }
  wake_lock_->RequestWakeLock();
}

}  // namespace borealis
