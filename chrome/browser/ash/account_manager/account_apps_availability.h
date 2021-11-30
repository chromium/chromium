// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_

#include "components/keyed_service/core/keyed_service.h"

namespace ash {

// This class is tracking which accounts from `AccountManager` should be
// available in apps. Currently only availability in ARC++ is being tracked.
// ARC++ availability may be set just after account addition or when user
// changes it manually in OS Settings.
// There should be only one instance of this class, which is attached to the
// only regular Ash profile. The class should exist only if Account Manager
// exists (if `ash::IsAccountManagerAvailable(profile)` is `true`).
class AccountAppsAvailability : public KeyedService {
 public:
  AccountAppsAvailability();
  ~AccountAppsAvailability() override;

  AccountAppsAvailability(const AccountAppsAvailability&) = delete;
  AccountAppsAvailability& operator=(const AccountAppsAvailability&) = delete;

  // Returns `true` if `kArcAccountRestrictions` and `kLacrosSupport` are
  // enabled.
  static bool IsArcAccountRestrictionsEnabled();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_H_
