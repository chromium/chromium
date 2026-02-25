// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SAML_CONFIRM_PASSWORD_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SAML_CONFIRM_PASSWORD_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/login/base_screen_handler_utils.h"

namespace ash {

class SamlConfirmPasswordView;
class WizardContext;

// Context for SAML password confirmation, holding data copied from UserContext.
// Isolates scraped passwords and account ID for use during the SAML confirm
// password flow logic.
struct SamlContext {
  const std::vector<std::string> scraped_saml_passwords;
  const AccountId account_id;

  SamlContext(std::vector<std::string> scraped_saml_passwords,
              AccountId account_id);
  ~SamlContext();
  SamlContext(const SamlContext&) = delete;
  SamlContext& operator=(const SamlContext&) = delete;
};

// This class represents GAIA screen: login screen that is responsible for
// GAIA-based sign-in.
class SamlConfirmPasswordScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = SamlConfirmPasswordView;

  enum class Result { kSuccess, kCancel, kTooManyAttempts, kNotApplicable };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  SamlConfirmPasswordScreen(base::WeakPtr<SamlConfirmPasswordView> view,
                            const ScreenExitCallback& exit_callback);

  SamlConfirmPasswordScreen(const SamlConfirmPasswordScreen&) = delete;
  SamlConfirmPasswordScreen& operator=(const SamlConfirmPasswordScreen&) =
      delete;

  ~SamlConfirmPasswordScreen() override;

  void SetContextAndPasswords(std::unique_ptr<UserContext> user_context,
                              ::login::StringList scraped_saml_passwords);

  bool MaybeSkip(WizardContext& context) override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::ListValue& args) override;

  void TryPassword(const std::string& password);
  void ShowPasswordStep(bool retry);

  void ObtainContextAndStoreSamlPassword(const std::string password);
  void SetPasswordAndReturnContextWithExitSuccess(
      const std::string password,
      std::unique_ptr<UserContext> user_context);
  void ResetSecretsAndExit();

  void InspectContextAndShowImpl(UserContext* user_context);
  void ShowImplInternal(AccountId account_id,
                        std::vector<std::string> scraped_saml_passwords);

  base::WeakPtr<SamlConfirmPasswordView> view_;

  ScreenExitCallback exit_callback_;

  std::unique_ptr<const SamlContext> saml_context_;

  // TODO: b/481969867 - Remove user_context_ and scraped_saml_passwords_ and
  // corresponding code after managed local pin and password feature launch.
  std::unique_ptr<UserContext> user_context_;
  ::login::StringList scraped_saml_passwords_;
  int attempt_count_ = 0;
  base::WeakPtrFactory<SamlConfirmPasswordScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SAML_CONFIRM_PASSWORD_SCREEN_H_
