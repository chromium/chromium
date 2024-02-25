// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_GAIA_PASSWORD_CHANGED_SCREEN_LEGACY_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_GAIA_PASSWORD_CHANGED_SCREEN_LEGACY_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "components/account_id/account_id.h"

namespace ash {

class GaiaPasswordChangedView;

// Controller for the tpm error screen.
class GaiaPasswordChangedScreenLegacy : public BaseScreen {
 public:
  using TView = GaiaPasswordChangedView;

  enum class Result {
    CANCEL = 0,
    RESYNC = 1,
    MIGRATE = 2,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  explicit GaiaPasswordChangedScreenLegacy(
      const ScreenExitCallback& exit_callback,
      base::WeakPtr<GaiaPasswordChangedView> view);
  GaiaPasswordChangedScreenLegacy(const GaiaPasswordChangedScreenLegacy&) =
      delete;
  GaiaPasswordChangedScreenLegacy& operator=(
      const GaiaPasswordChangedScreenLegacy&) = delete;
  ~GaiaPasswordChangedScreenLegacy() override;

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted.  Only additions possible.
  enum class UserAction {
    kResyncUserData = 0,
    kMigrateUserData = 1,
    kCancel = 2,
    kIncorrectOldPassword = 3,
    kMaxValue = kIncorrectOldPassword
  };

  void Configure(const AccountId& account_id, bool after_incorrect_attempt);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void MigrateUserData(const std::string& old_password);
  void CancelPasswordChangedFlow();
  void OnCookiesCleared();

  AccountId account_id_;
  bool show_error_ = false;

  base::WeakPtr<GaiaPasswordChangedView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<GaiaPasswordChangedScreenLegacy> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_GAIA_PASSWORD_CHANGED_SCREEN_LEGACY_H_
