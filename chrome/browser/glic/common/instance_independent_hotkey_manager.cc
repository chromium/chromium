// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/instance_independent_hotkey_manager.h"

#include "chrome/browser/glic/common/application_hotkey_delegate.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"

namespace glic {

InstanceIndependentHotkeyManager::InstanceIndependentHotkeyManager(
    GlicInstanceCoordinatorImpl* coordinator)
    : coordinator_(coordinator) {
  hotkey_manager_ = std::make_unique<LocalHotkeyManager>(
      std::make_unique<ApplicationScopedRegistrationDelegate>(), this,
      base::span<const LocalHotkeyManager::Command>());
  hotkey_manager_->InitializeAccelerators();
}

InstanceIndependentHotkeyManager::~InstanceIndependentHotkeyManager() = default;

bool InstanceIndependentHotkeyManager::AcceleratorPressed(
    LocalHotkeyManager::Command command) {
  // TODO(b/512148847): Handle commands here when supported.
  return false;
}

bool InstanceIndependentHotkeyManager::CanHandleAccelerators() const {
  // TODO(b/512148847): Update this when commands are supported.
  return false;
}

}  // namespace glic
