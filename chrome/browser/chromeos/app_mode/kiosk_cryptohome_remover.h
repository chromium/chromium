// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_CRYPTOHOME_REMOVER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_CRYPTOHOME_REMOVER_H_

#include <vector>

#include "base/macros.h"

class AccountId;
class PrefRegistrySimple;

namespace chromeos {

// Helper functions to remove cryptohomes of no longer existing kiosk apps.
class KioskCryptohomeRemover {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);
  // Removes the cryptohomes of kiosks that were removed from policy.
  static void RemoveObsoleteCryptohomes();
  // Cancels the scheduled next-startup removal of the kiosk app.
  static void CancelDelayedCryptohomeRemoval(const AccountId& id);
  // Tries to remove cryptohomes of the list of users. For current active user,
  // remembers to do so on next boot and then terminates the session.
  static void RemoveCryptohomesAndExitIfNeeded(
      const std::vector<AccountId>& account_ids);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(KioskCryptohomeRemover);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_CRYPTOHOME_REMOVER_H_
