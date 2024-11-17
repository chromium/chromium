// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_

#include <string>

#include "ash/public/cpp/screen_backlight_observer.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/login/gaia_reauth_token_fetcher.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"

namespace policy {
struct AccountStatus;
class AccountStatusCheckFetcher;
}  // namespace policy

namespace ash {

class GaiaView;

// This class represents GAIA screen: login screen that is responsible for
// GAIA-based sign-in. Screen observs backlight to turn the camera off if the
// device screen is not ON.
class GaiaScreen : public BaseScreen, public ScreenBacklightObserver {
 public:
  using TView = GaiaView;

  enum class Result {
    BACK,
    // used to distinguish clicking back on the child sign-in/sign-up screen
    // vs. default Gaia screen
    BACK_CHILD,
    CANCEL,
    ENTERPRISE_ENROLL,
    ENTER_QUICK_START,
    QUICK_START_ONGOING,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  GaiaScreen(base::WeakPtr<TView> view,
             const ScreenExitCallback& exit_callback);

  GaiaScreen(const GaiaScreen&) = delete;
  GaiaScreen& operator=(const GaiaScreen&) = delete;

  ~GaiaScreen() override;

  // Loads online GAIA into the webview.
  void LoadOnlineGaia();
  // Reset authenticator.
  void Reset();
  // Calls authenticator reload on JS side.
  void ReloadGaiaAuthenticator();

  const std::string& EnrollmentNudgeEmail();

  // ScreenBacklightObserver:
  void OnScreenBacklightStateChanged(
      ScreenBacklightState screen_backlight_state) override;

 private:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;
  void HandleIdentifierEntered(const std::string& account_identifier);

  void OnGetAuthFactorsConfiguration(std::unique_ptr<UserContext> user_context,
                                     std::optional<AuthenticationError> error);
  // Fetch Gaia reauth request token from the recovery service.
  void FetchGaiaReauthToken(const AccountId& account);
  void OnGaiaReauthTokenFetched(const AccountId& account,
                                const std::string& token);
  void OnAccountStatusFetched(const std::string& user_email,
                              bool result,
                              policy::AccountStatus status);

  // Triggers the enrollment nudge flow and returns true if all requirements are
  // met, otherwise does nothing and returns false.
  bool ShouldFetchEnrollmentNudgePolicy(const std::string& user_email);

  // Called when quick start button is clicked.
  void OnQuickStartButtonClicked();
  void SetQuickStartButtonVisibility(bool visible);

  // Starts online authentication for a given account (can be empty if
  // user is unknown). If `force_default_gaia_page` is true, will
  // choose the Gaia path corresponding to
  // `WizardContext::GaiaPath::kDefault`.
  void LoadOnlineGaiaForAccount(const AccountId& account,
                                bool force_default_gaia_page = false);

  // Whether the QuickStart entry point visibility has already been determined.
  // This flag prevents duplicate histogram entries.
  bool has_emitted_quick_start_visible = false;

  AuthFactorEditor auth_factor_editor_;
  std::unique_ptr<GaiaReauthTokenFetcher> gaia_reauth_token_fetcher_;
  std::unique_ptr<policy::AccountStatusCheckFetcher> account_status_fetcher_;

  base::WeakPtr<TView> view_;

  ScreenExitCallback exit_callback_;

  base::ScopedObservation<BacklightsForcedOffSetter, ScreenBacklightObserver>
      backlights_forced_off_observation_{this};

  // Used to cache email between "identifierEntered" event and a switch to
  // enrollment screen.
  std::string enrollment_nudge_email_;

  base::WeakPtrFactory<GaiaScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
