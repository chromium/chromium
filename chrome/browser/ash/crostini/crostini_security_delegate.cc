// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_security_delegate.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace crostini {

void CrostiniSecurityDelegate::Build(
    Profile* profile,
    std::string vm_name,
    base::OnceCallback<void(std::unique_ptr<guest_os::GuestOsSecurityDelegate>)>
        callback) {
  std::string reason;
  if (!CrostiniFeatures::Get()->IsAllowedNow(profile, &reason)) {
    LOG(WARNING) << "Crostini is not allowed: " << reason;
    std::move(callback).Run(nullptr);
    return;
  }
  // WrapUnique is used because the constructor is private.
  std::move(callback).Run(
      base::WrapUnique(new CrostiniSecurityDelegate(std::move(vm_name))));
}

CrostiniSecurityDelegate::~CrostiniSecurityDelegate() = default;

bool CrostiniSecurityDelegate::CanLockPointer(aura::Window* window) const {
  return window->GetProperty(chromeos::kUseOverviewToExitPointerLock);
}

}  // namespace crostini
