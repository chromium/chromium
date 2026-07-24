// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/view_scoped_registration_delegate.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/android/accelerator_manager_android.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"

namespace glic {

namespace {
class AndroidGlicPanelScopedHotkeyRegistration
    : public LocalHotkeyManager::ScopedHotkeyRegistration {
 public:
  AndroidGlicPanelScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target,
      base::WeakPtr<LocalHotkeyManager::Panel> panel)
      : accelerator_(accelerator), target_(target), panel_(panel) {
    CHECK(!accelerator_.IsEmpty());
    RegisterAccelerator();
  }

  ~AndroidGlicPanelScopedHotkeyRegistration() override {
    UnregisterAccelerator();
  }

 private:
  ui::AcceleratorManagerAndroid* GetAcceleratorManager() {
    if (!target_ || !panel_) {
      return nullptr;
    }
    auto* browser_window = panel_->GetBrowserWindowInterface();
    if (!browser_window) {
      return nullptr;
    }
    auto* window = browser_window->GetWindow();
    if (!window) {
      return nullptr;
    }
    return ui::AcceleratorManagerAndroid::FromWindow(window->GetNativeWindow());
  }

  void RegisterAccelerator() {
    auto* accelerator_manager = GetAcceleratorManager();
    if (!accelerator_manager) {
      return;
    }
    accelerator_manager->RegisterAccelerator(
        accelerator_, ui::AcceleratorManager::HandlerPriority::kNormalPriority,
        target_.get());
  }

  void UnregisterAccelerator() {
    auto* accelerator_manager = GetAcceleratorManager();
    if (!accelerator_manager) {
      return;
    }
    accelerator_manager->UnregisterAccelerator(accelerator_, target_.get());
  }

  ui::Accelerator accelerator_;
  base::WeakPtr<ui::AcceleratorTarget> target_;
  base::WeakPtr<LocalHotkeyManager::Panel> panel_;
};
}  // namespace

ViewScopedRegistrationDelegate::ViewScopedRegistrationDelegate(
    base::WeakPtr<LocalHotkeyManager::Panel> panel)
    : panel_(panel) {}

ViewScopedRegistrationDelegate::~ViewScopedRegistrationDelegate() = default;

std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
ViewScopedRegistrationDelegate::CreateScopedHotkeyRegistration(
    ui::Accelerator accelerator,
    base::WeakPtr<ui::AcceleratorTarget> target) {
  if (!panel_) {
    return nullptr;
  }
  return std::make_unique<AndroidGlicPanelScopedHotkeyRegistration>(
      accelerator, target, panel_);
}

}  // namespace glic
