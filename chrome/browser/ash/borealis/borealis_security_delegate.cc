// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_security_delegate.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace borealis {

void BorealisSecurityDelegate::Build(
    Profile* profile,
    base::OnceCallback<void(std::unique_ptr<guest_os::GuestOsSecurityDelegate>)>
        callback) {
  BorealisService::GetForProfile(profile)->Features().IsAllowed(base::BindOnce(
      [](Profile* profile,
         base::OnceCallback<void(
             std::unique_ptr<guest_os::GuestOsSecurityDelegate>)> callback,
         BorealisFeatures::AllowStatus allow_status) {
        if (allow_status != BorealisFeatures::AllowStatus::kAllowed) {
          LOG(WARNING) << "Borealis is not allowed: " << allow_status;
          std::move(callback).Run(nullptr);
          return;
        }
        BorealisSecurityDelegate* delegate =
            new BorealisSecurityDelegate(profile);
        // Use WrapUnique due to private constructor.
        std::move(callback).Run(base::WrapUnique(delegate));
      },
      profile, std::move(callback)));
}

BorealisSecurityDelegate::~BorealisSecurityDelegate() = default;

bool BorealisSecurityDelegate::CanSelfActivate(aura::Window* window) const {
  return BorealisService::GetForProfile(profile_)
             ->WindowManager()
             .GetShelfAppId(window) == kClientAppId;
}

bool BorealisSecurityDelegate::CanLockPointer(aura::Window* window) const {
  return window->GetProperty(chromeos::kUseOverviewToExitPointerLock);
}

bool BorealisSecurityDelegate::CanSetBoundsWithServerSideDecoration(
    aura::Window* window) const {
  return true;
}

// static
std::unique_ptr<BorealisSecurityDelegate>
BorealisSecurityDelegate::MakeForTesting(Profile* profile) {
  BorealisSecurityDelegate* delegate = new BorealisSecurityDelegate(profile);
  // Use WrapUnique due to private constructor.
  return base::WrapUnique(delegate);
}

BorealisSecurityDelegate::BorealisSecurityDelegate(Profile* profile)
    : profile_(profile) {}

}  // namespace borealis
