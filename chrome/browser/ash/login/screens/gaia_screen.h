// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_

#include <string>

#include "ash/public/cpp/screen_backlight_observer.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_client_cert_usage_observer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/online_login_helper.h"
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
    LOGIN_SUCCESS,
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

  // Handle user actions.
  void HandleCompleteAuthentication(
      const std::string& gaia_id,
      const std::string& email,
      const std::string& password,
      const base::Value::List& scraped_saml_passwords_value,
      bool using_saml,
      const base::Value::List& services_list,
      bool services_provided,
      const base::Value::Dict& password_attributes,
      const base::Value::Dict& sync_trusted_vault_keys);
  // TODO(yunkez): This is only used in `Oobe.loginForTesting` in tast tests. We
  // could remove this or use HandleCompleteAuthentication instead.
  void HandleCompleteLoginForTesting(const std::string& gaia_id,
                                     const std::string& typed_email,
                                     const std::string& password,
                                     bool using_saml);
  void HandleIdentifierEntered(const std::string& account_identifier);
  void HandlePasswordEntered();
  void HandleGaiaLoaded();

  // Handles SAML/GAIA login flow metrics
  // is_third_party_idp == false means GAIA-based authentication
  void HandleUsingSAMLAPI(bool is_third_party_idp);

  void LoadGaiaAsync(const AccountId& account_id);

  // Updates the member variable and UMA histogram indicating whether the
  // Chrome Credentials Passing API was used during SAML login.
  void OnSamlPrincipalsAPIUsed(bool is_third_party_idp, bool is_api_used);

  void RecordScrapedPasswordCount(int password_count);
  bool IsSamlUserPasswordless();

  void OnCookieWaitTimeout();

  void OnCompleteLogin(std::unique_ptr<UserContext> user_context);
  void SAMLConfirmPassword(::login::StringList scraped_saml_passwords,
                           std::unique_ptr<UserContext> user_context);

  // This flag is set when user authenticated using the Chrome Credentials
  // Passing API (the login could happen via SAML or, with the current
  // server-side implementation, via Gaia).
  bool using_saml_api_ = false;

  std::unique_ptr<LoginClientCertUsageObserver>
      extension_provided_client_cert_usage_observer_;

  std::unique_ptr<OnlineLoginHelper> online_login_helper_;

  GaiaView::GaiaLoginVariant login_request_variant_ =
      GaiaView::GaiaLoginVariant::kUnknown;

  // Used to record amount of time user needed for successful online login.
  std::unique_ptr<base::ElapsedTimer> elapsed_timer_;

  base::WeakPtr<TView> view_;

  ScreenExitCallback exit_callback_;

  base::ScopedObservation<BacklightsForcedOffSetter, ScreenBacklightObserver>
      backlights_forced_off_observation_{this};

  base::WeakPtrFactory<GaiaScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
