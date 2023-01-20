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
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"

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
    CANCEL,
    ENTERPRISE_ENROLL,
    START_CONSUMER_KIOSK,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  GaiaScreen(base::WeakPtr<TView> view,
             const ScreenExitCallback& exit_callback);

  GaiaScreen(const GaiaScreen&) = delete;
  GaiaScreen& operator=(const GaiaScreen&) = delete;

  ~GaiaScreen() override;

  // Loads online Gaia into the webview.
  void LoadOnline(const AccountId& account);
  // Loads online Gaia (for child signup) into the webview.
  void LoadOnlineForChildSignup();
  // Loads online Gaia (for child signin) into the webview.
  void LoadOnlineForChildSignin();
  void ShowAllowlistCheckFailedError();
  // Reset authenticator.
  void Reset();
  // Calls authenticator reload on JS side.
  void ReloadGaiaAuthenticator();

  // ScreenBacklightObserver:
  void OnScreenBacklightStateChanged(
      ScreenBacklightState screen_backlight_state) override;

 private:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  void OnGetAuthFactorsConfiguration(std::unique_ptr<UserContext> user_context,
                                     absl::optional<AuthenticationError> error);
  void OnGaiaReauthTokenFetched(const AccountId& account,
                                const std::string& token);

  AuthFactorEditor auth_factor_editor_;
  std::unique_ptr<GaiaReauthTokenFetcher> gaia_reauth_token_fetcher_;

  base::WeakPtr<TView> view_;

  ScreenExitCallback exit_callback_;

  base::ScopedObservation<BacklightsForcedOffSetter, ScreenBacklightObserver>
      backlights_forced_off_observation_{this};

  base::WeakPtrFactory<GaiaScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
