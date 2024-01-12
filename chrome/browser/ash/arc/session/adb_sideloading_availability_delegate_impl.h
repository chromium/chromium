// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ADB_SIDELOADING_AVAILABILITY_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ADB_SIDELOADING_AVAILABILITY_DELEGATE_IMPL_H_

#include "ash/components/arc/session/adb_sideloading_availability_delegate.h"
#include "base/memory/raw_ptr.h"

#include "chrome/browser/profiles/profile.h"

namespace arc {

class AdbSideloadingAvailabilityDelegateImpl
    : public AdbSideloadingAvailabilityDelegate {
 public:
  AdbSideloadingAvailabilityDelegateImpl();

  // Not copyable or movable
  AdbSideloadingAvailabilityDelegateImpl(
      const AdbSideloadingAvailabilityDelegateImpl&) = delete;
  AdbSideloadingAvailabilityDelegateImpl& operator=(
      const AdbSideloadingAvailabilityDelegateImpl&) = delete;

  ~AdbSideloadingAvailabilityDelegateImpl() override;

  void SetProfile(Profile* profile);

  void CanChangeAdbSideloading(
      base::OnceCallback<void(bool can_change_adb_sideloading)> callback)
      override;

 private:
  // Unowned pointer. Keeps current profile.
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ADB_SIDELOADING_AVAILABILITY_DELEGATE_IMPL_H_
