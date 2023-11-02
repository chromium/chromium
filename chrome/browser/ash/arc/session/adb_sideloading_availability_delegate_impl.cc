// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/adb_sideloading_availability_delegate_impl.h"

#include "chrome/browser/ash/crostini/crostini_features.h"

namespace arc {

AdbSideloadingAvailabilityDelegateImpl::
    AdbSideloadingAvailabilityDelegateImpl() = default;

AdbSideloadingAvailabilityDelegateImpl::
    ~AdbSideloadingAvailabilityDelegateImpl() = default;

void AdbSideloadingAvailabilityDelegateImpl::SetProfile(Profile* profile) {
  profile_ = profile;
}

void AdbSideloadingAvailabilityDelegateImpl::CanChangeAdbSideloading(
    base::OnceCallback<void(bool can_change_adb_sideloading)> callback) {
  if (!profile_) {
    // If |profile_| is not set, return immediately and mark adb sideloading as
    // not allowed
    std::move(callback).Run(false);
    return;
  }

  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, std::move(callback));
}

}  // namespace arc
