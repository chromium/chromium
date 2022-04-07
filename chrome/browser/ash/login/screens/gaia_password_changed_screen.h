// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_PASSWORD_CHANGED_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_PASSWORD_CHANGED_SCREEN_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/gaia_password_changed_screen_handler.h"
#include "components/account_id/account_id.h"

namespace ash {

// Controller for the tpm error screen.
class GaiaPasswordChangedScreen : public BaseScreen {
 public:
  using TView = GaiaPasswordChangedView;

  enum class Result {
    CANCEL,
    RESYNC,
    MIGRATE,
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  explicit GaiaPasswordChangedScreen(const ScreenExitCallback& exit_callback,
                                     GaiaPasswordChangedView* view);
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
    kMaxValue = kIncorrectOldPassword
  };

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(GaiaPasswordChangedView* view);

  void MigrateUserData(const std::string& old_password);

  void Configure(const AccountId& account_id, bool after_incorrect_attempt);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserActionDeprecated(const std::string& action_id) override;

  void CancelPasswordChangedFlow();
  void OnCookiesCleared();

  AccountId account_id_;
  bool show_error_ = false;

  GaiaPasswordChangedView* view_ = nullptr;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<GaiaPasswordChangedScreen> weak_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::GaiaPasswordChangedScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_PASSWORD_CHANGED_SCREEN_H_
