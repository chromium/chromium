// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_security_delegate.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace bruschetta {

void BruschettaSecurityDelegate::Build(
    Profile* profile,
    const std::string& vm_name,
    base::OnceCallback<void(std::unique_ptr<guest_os::GuestOsSecurityDelegate>)>
        callback) {
  if (!bruschetta::IsAllowed(profile, vm_name)) {
    LOG(WARNING) << "Bruschetta is not allowed";
    std::move(callback).Run(nullptr);
    return;
  }
  // WrapUnique is used because the constructor is private.
  std::move(callback).Run(base::WrapUnique(new BruschettaSecurityDelegate()));
}

BruschettaSecurityDelegate::~BruschettaSecurityDelegate() = default;

bool BruschettaSecurityDelegate::CanLockPointer(aura::Window* window) const {
  return window->GetProperty(chromeos::kUseOverviewToExitPointerLock);
}

}  // namespace bruschetta
