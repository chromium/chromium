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
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefChangeRegistrar;
class PrefService;
class Profile;

namespace ash {

class LocalAuthFactorsPolicyControllerTest;

// Manages and enforces policies related to local authentication factors.
// This class monitors changes in allowed local auth factors and their
// complexity requirements. It ensures compliance by potentially forcing
// an online sign-in (if no allowed factors are configured) or showing
// a persistent notification (if complexity requirements have increased).
class LocalAuthFactorsPolicyController
    : public KeyedService,
      public ash::auth::mojom::FactorObserver {
 public:
  LocalAuthFactorsPolicyController(PrefService& local_state,
                                   Profile* profile,
                                   const AccountId& account_id);
  ~LocalAuthFactorsPolicyController() override;

  // Sets the callback that is called every time the pref is processed.
  static void SetPrefProcessedCallbackForTesting(
      base::RepeatingClosure on_pref_processed);

  // Sets the callback that is called every time a notification is shown.
  static void SetNotificationShownCallbackForTesting(
      base::RepeatingClosure on_notification_shown);

 private:
  PrefService& prefs();

  void OnAllowedAuthFactorsPrefUpdated();
  void OnGetAuthFactorsConfiguration(
      base::ScopedClosureRunner pref_processed_runner,
      std::unique_ptr<ash::UserContext> user_context,
      std::optional<ash::AuthenticationError> error);
  AuthFactorEditor* GetAuthFactorEditor();
  std::optional<ash::AuthFactorsSet> GetAllowedAuthFactors();

  // ash::auth::mojom::FactorObserver:
  void OnFactorChanged(ash::auth::mojom::AuthFactor factor) override;

  void OnComplexityPrefUpdated();
  void ShowComplexityUpdateNotification();
  void OnShowComplexityUpdateNotification(
      std::unique_ptr<ash::UserContext> user_context,
      std::optional<ash::AuthenticationError> error);
  void DismissComplexityUpdateNotification();

  friend class LocalAuthFactorsPolicyControllerTest;

  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<ash::AuthFactorEditor> auth_factor_editor_;
  const raw_ptr<Profile> profile_;
  const AccountId account_id_;

  mojo::Receiver<ash::auth::mojom::FactorObserver> receiver_{this};

  base::WeakPtrFactory<LocalAuthFactorsPolicyController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_POLICY_CONTROLLER_H_
