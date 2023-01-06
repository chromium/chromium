// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ADB_SIDELOADING_AVAILABILITY_DELEGATE_H_
#define ASH_COMPONENTS_ARC_SESSION_ADB_SIDELOADING_AVAILABILITY_DELEGATE_H_

#include "base/functional/callback_forward.h"

namespace arc {

// An abstract class describing a delegate that can fetch the availability of
// ADB sideloading. It is used to inject the dependency of the ARC component to
// ArcSessionManager which extends this abstract class and accesses the
// sideloading availability status of CrostiniFeatures.
// TODO(b/161813141): Refactor this to remove the dependency
class AdbSideloadingAvailabilityDelegate {
 public:
  virtual ~AdbSideloadingAvailabilityDelegate() = default;

  // Fetches the ADB sideloading availability value
  virtual void CanChangeAdbSideloading(
      base::OnceCallback<void(bool can_change_adb_sideloading)> callback) = 0;
};
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ADB_SIDELOADING_AVAILABILITY_DELEGATE_H_
