// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_CRYPTOHOME_REMOVER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_CRYPTOHOME_REMOVER_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"

class AccountId;
class PrefRegistrySimple;
class PrefService;

namespace ash {

// Removes cryptohomes of no longer existing kiosk apps.
class KioskCryptohomeRemover {
 public:
  explicit KioskCryptohomeRemover(PrefService* local_state);
  KioskCryptohomeRemover(const KioskCryptohomeRemover&) = delete;
  KioskCryptohomeRemover& operator=(const KioskCryptohomeRemover&) = delete;
  ~KioskCryptohomeRemover();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Removes the cryptohomes of kiosks that were removed from policy.
  void RemoveObsoleteCryptohomes();

  // Cancels the scheduled next-startup removal of the kiosk app.
  void CancelDelayedCryptohomeRemoval(const AccountId& account_id);

  // Tries to remove cryptohomes of the list of users. For current active user,
  // remembers to do so on next boot and then terminates the session.
  void RemoveCryptohomesAndExitIfNeeded(
      const std::vector<AccountId>& account_ids);

 private:
  void PerformDelayedCryptohomeRemovals(bool service_is_available);

  void OnRemoveAppCryptohomeComplete(
      const cryptohome::Identification& id,
      base::OnceClosure callback,
      std::optional<user_data_auth::RemoveReply> reply);

  void ScheduleDelayedCryptohomeRemoval(const AccountId& account_id);

  void UnscheduleDelayedCryptohomeRemoval(const cryptohome::Identification& id);

  const raw_ref<PrefService> local_state_;
  base::WeakPtrFactory<KioskCryptohomeRemover> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_CRYPTOHOME_REMOVER_H_
