// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GAIA_PASSWORD_CHANGED_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GAIA_PASSWORD_CHANGED_SCREEN_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "components/account_id/account_id.h"

namespace chromeos {

class GaiaPasswordChangedView;

// Controller for the tpm error screen.
class GaiaPasswordChangedScreen : public BaseScreen {
 public:
  using TView = GaiaPasswordChangedView;

  explicit GaiaPasswordChangedScreen(GaiaPasswordChangedView* view);
  GaiaPasswordChangedScreen(const GaiaPasswordChangedScreen&) = delete;
  GaiaPasswordChangedScreen& operator=(const GaiaPasswordChangedScreen&) =
      delete;
  ~GaiaPasswordChangedScreen() override;

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(GaiaPasswordChangedView* view);

  void MigrateUserData(const std::string& old_password);

  void Configure(const AccountId& account_id, bool after_incorrect_attempt);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  void CancelPasswordChangedFlow();
  void OnCookiesCleared();

  AccountId account_id_;
  bool show_error_ = false;

  GaiaPasswordChangedView* view_ = nullptr;

  base::WeakPtrFactory<GaiaPasswordChangedScreen> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_GAIA_PASSWORD_CHANGED_SCREEN_H_
