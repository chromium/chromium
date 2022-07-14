// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_security_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "third_party/cros_system_api/constants/vm_tools.h"

namespace borealis {

void BorealisSecurityDelegate::Build(
    Profile* profile,
    base::OnceCallback<void(std::unique_ptr<guest_os::GuestOsSecurityDelegate>)>
        callback) {
  BorealisService::GetForProfile(profile)->Features().IsAllowed(base::BindOnce(
      [](base::OnceCallback<void(
             std::unique_ptr<guest_os::GuestOsSecurityDelegate>)> callback,
         BorealisFeatures::AllowStatus allow_status) {
        if (allow_status != BorealisFeatures::AllowStatus::kAllowed) {
          LOG(WARNING) << "Borealis is not allowed: " << allow_status;
          std::move(callback).Run(nullptr);
          return;
        }
        std::move(callback).Run(std::make_unique<BorealisSecurityDelegate>());
      },
      std::move(callback)));
}

BorealisSecurityDelegate::~BorealisSecurityDelegate() = default;

std::string BorealisSecurityDelegate::GetSecurityContext() const {
  return vm_tools::kConciergeSecurityContext;
}

}  // namespace borealis
