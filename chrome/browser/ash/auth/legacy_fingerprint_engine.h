// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AUTH_LEGACY_FINGERPRINT_ENGINE_H_
#define CHROME_BROWSER_ASH_AUTH_LEGACY_FINGERPRINT_ENGINE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"

class AccountId;
class PrefService;

namespace ash {

class UserContext;
class AuthPerformer;

// Handles Fingerprint authentication operations and is the source of truth for
// the availability of Fingerprint authentication.
class LegacyFingerprintEngine {
 public:
  enum class Purpose { kAny, kUnlock, kWebAuthn };

  explicit LegacyFingerprintEngine(ash::AuthPerformer* auth_performer);
  LegacyFingerprintEngine(const LegacyFingerprintEngine&) = delete;
  LegacyFingerprintEngine& operator=(const LegacyFingerprintEngine&) = delete;
  virtual ~LegacyFingerprintEngine();

  LegacyFingerprintEngine::Purpose FromQuickUnlockPurpose(
      quick_unlock::Purpose purpose) const;

  quick_unlock::Purpose ToQuickUnlockPurpose(
      LegacyFingerprintEngine::Purpose purpose) const;

  bool IsFingerprintDisabledByPolicy(
      const PrefService& pref_service,
      LegacyFingerprintEngine::Purpose purpose) const;

  // Returns true if the device supports fingerprint authentication and
  // fingerprint is not disabled by policy, and not if the user associated with
  // `pref_service` has actually set up fingerprint as an auth factor. Returns
  // false otherwise.
  bool IsFingerprintEnabled(const PrefService& pref_service,
                            LegacyFingerprintEngine::Purpose purpose) const;

  // Returns true if the user associated with `account_id` has actually
  // configured fingerprint as an auth factor.
  bool IsFingerprintAvailable(Purpose purpose, const AccountId& account_id);

  void PrepareLegacyFingerprintFactor(std::unique_ptr<UserContext> user_context,
                                      AuthOperationCallback callback);

  void TerminateLegacyFingerprintFactor(
      std::unique_ptr<UserContext> user_context,
      AuthOperationCallback callback);

 private:
  // Non owning pointer
  const raw_ptr<AuthPerformer> auth_performer_;

  base::WeakPtrFactory<LegacyFingerprintEngine> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_AUTH_LEGACY_FINGERPRINT_ENGINE_H_
