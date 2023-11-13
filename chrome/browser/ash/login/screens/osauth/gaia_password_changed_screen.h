// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_GAIA_PASSWORD_CHANGED_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_GAIA_PASSWORD_CHANGED_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "components/account_id/account_id.h"

namespace ash {

class AuthFactorEditor;
class AuthPerformer;
class GaiaPasswordChangedView;
class MountPerformer;

// Controller for the tpm error screen.
class GaiaPasswordChangedScreen : public BaseScreen {
 public:
  using TView = GaiaPasswordChangedView;

  enum class Result {
    CANCEL,
    CONTINUE_LOGIN,
    RECREATE_USER,
    CRYPTOHOME_ERROR,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  explicit GaiaPasswordChangedScreen(
      const ScreenExitCallback& exit_callback,
      base::WeakPtr<GaiaPasswordChangedView> view);
  GaiaPasswordChangedScreen(const GaiaPasswordChangedScreen&) = delete;
  GaiaPasswordChangedScreen& operator=(const GaiaPasswordChangedScreen&) =
      delete;
  ~GaiaPasswordChangedScreen() override;

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted.  Only additions possible.
  enum class UserAction {
    kResyncUserData = 0,
    kMigrateUserData = 1,
    kCancel = 2,
    kIncorrectOldPassword = 3,
    kIgnoreRecovery = 4,
    kSetupRecovery = 5,
    kMaxValue = kSetupRecovery
  };

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void FinishWithResult(Result result);

  void AttemptAuthentication(const std::string& old_password);
  void OnPasswordAuthentication(std::unique_ptr<UserContext> user_context,
                                absl::optional<AuthenticationError> error);
  void OnGetConfiguration(std::unique_ptr<UserContext> user_context,
                          absl::optional<AuthenticationError> error);
  void OnPasswordUpdated(std::unique_ptr<UserContext> user_context,
                         absl::optional<AuthenticationError> error);
  void RecreateUser();
  void OnRemovedUserDirectory(std::unique_ptr<UserContext> user_context,
                              absl::optional<AuthenticationError> error);

  void CancelPasswordChangedFlow();
  void OnCookiesCleared();

  // Used for authentication attempt, cleared upon screen exit.
  std::unique_ptr<AuthPerformer> auth_performer_;
  // Used for changing password, cleared upon screen exit.
  std::unique_ptr<AuthFactorEditor> factor_editor_;
  // Used for deleting home directory, cleared upon screen exit.
  std::unique_ptr<MountPerformer> mount_performer_;

  base::WeakPtr<GaiaPasswordChangedView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<GaiaPasswordChangedScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_GAIA_PASSWORD_CHANGED_SCREEN_H_
