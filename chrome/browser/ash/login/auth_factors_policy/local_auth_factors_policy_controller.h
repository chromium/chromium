// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_POLICY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_POLICY_CONTROLLER_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash {

// Manages and enforces policies related to local authentication factors.
// This class monitors changes in allowed local auth factors preferences
// and ensures the user's auth setup complies with the current policy,
// potentially forcing an online sign-in if no allowed local factors are
// configured.
class LocalAuthFactorsPolicyController : public KeyedService {
 public:
  explicit LocalAuthFactorsPolicyController(PrefService& profile_pref_service,
                                            const AccountId& account_id);
  ~LocalAuthFactorsPolicyController() override;

  // Sets the callback that is called every time the pref is processed.
  static void SetPrefProcessedCallbackForTesting(
      base::RepeatingClosure on_pref_processed);

 private:
  PrefService& prefs();

  void OnAllowedAuthFactorsPrefUpdated();
  void OnGetAuthFactorsConfiguration(
      base::ScopedClosureRunner pref_processed_runner,
      std::unique_ptr<ash::UserContext> user_context,
      std::optional<ash::AuthenticationError> error);
  AuthFactorEditor* GetAuthFactorEditor();
  std::optional<ash::AuthFactorsSet> GetAllowedAuthFactors();

  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<ash::AuthFactorEditor> auth_factor_editor_;
  const AccountId account_id_;

  base::WeakPtrFactory<LocalAuthFactorsPolicyController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_POLICY_CONTROLLER_H_
