// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_ACTIVATION_NECESSITY_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_ACTIVATION_NECESSITY_CHECKER_H_

#include "base/callback.h"

class Profile;

namespace arc {

class AdbSideloadingAvailabilityDelegate;

// ArcActivationNecessityChecker checks if it's necessary to activate ARC
// without the user's action.
class ArcActivationNecessityChecker {
 public:
  ArcActivationNecessityChecker(Profile* profile,
                                AdbSideloadingAvailabilityDelegate*
                                    adb_sideloading_availability_delegate);
  ArcActivationNecessityChecker(const ArcActivationNecessityChecker&) = delete;
  ArcActivationNecessityChecker& operator=(
      const ArcActivationNecessityChecker&) = delete;
  ~ArcActivationNecessityChecker();

  // Checks if it's necessary to activate ARC without the user's action, and
  // runs the callback with true if it's necessary to activate ARC.
  using CheckCallback = base::OnceCallback<void(bool result)>;
  void Check(CheckCallback callback);

 private:
  Profile* const profile_;
  AdbSideloadingAvailabilityDelegate* const
      adb_sideloading_availability_delegate_;  // Owned by ArcSessionManager.
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_ACTIVATION_NECESSITY_CHECKER_H_
