// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"
#include "components/prefs/pref_service.h"

namespace ash {

class RemoveLocalAuthFactorsScreenView;
// Screen that is shown when the user goes through remove local auth factors
// flow
class RemoveLocalAuthFactorsScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = RemoveLocalAuthFactorsScreenView;

  enum class Result { kSuccess = 0, kError };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  RemoveLocalAuthFactorsScreen(
      PrefService* local_state,
      base::WeakPtr<RemoveLocalAuthFactorsScreenView> view,
      const ScreenExitCallback& exit_callback);

  RemoveLocalAuthFactorsScreen(const RemoveLocalAuthFactorsScreen&) = delete;
  RemoveLocalAuthFactorsScreen& operator=(const RemoveLocalAuthFactorsScreen&) =
      delete;

  ~RemoveLocalAuthFactorsScreen() override;

  // Replaces the local password if present with the online password, and
  // removes the PIN if present.
  void SetOnlinePasswordAndRemoveLocalAuthFactors();
  void RemoveLocalAuthFactors(auth::mojom::ConfigureResult result);
  void OnPinRemoved(auth::mojom::ConfigureResult result);
  void ShowRemoveLocalAuthFactorsSucess();

  // Callback called when the AuthFactorsConfiguration is fetched by the
  // `AuthFactorEditor`.
  void OnGetAuthFactorsConfiguration(
      std::unique_ptr<ash::UserContext> user_context,
      std::optional<ash::AuthenticationError> error);

  static std::string GetResultString(Result result);

 private:
  void InspectContext(UserContext* user_context);
  AuthFactorEditor* GetAuthFactorEditor();

  // BaseOSAuthSetupScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::ListValue& args) override;

  AuthFactorsConfiguration auth_factors_config_;
  std::optional<OnlinePassword> online_password_;
  const raw_ref<PrefService> local_state_;
  std::unique_ptr<ash::AuthFactorEditor> auth_factor_editor_;

  base::WeakPtr<RemoveLocalAuthFactorsScreenView> view_;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<RemoveLocalAuthFactorsScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_H_
