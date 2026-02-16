// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_PASSWORD_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_PASSWORD_SETUP_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"

class PrefService;

namespace ash {

class LocalPasswordSetupView;

class LocalPasswordSetupScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = LocalPasswordSetupView;

  enum class Result {
    kDone,
    kBack,
    kNotApplicable,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  // `local_state` must be non-null and must outlive `this`.
  LocalPasswordSetupScreen(PrefService* local_state,
                           base::WeakPtr<LocalPasswordSetupView> view,
                           const ScreenExitCallback& exit_callback);

  LocalPasswordSetupScreen(const LocalPasswordSetupScreen&) = delete;
  LocalPasswordSetupScreen& operator=(const LocalPasswordSetupScreen&) = delete;

  ~LocalPasswordSetupScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void DoShow();
  void OnUserAction(const base::ListValue& args) override;

  void OnUpdateLocalPassword(auth::mojom::ConfigureResult result);
  void OnSetLocalPassword(auth::mojom::ConfigureResult result);

  const raw_ref<PrefService> local_state_;

  base::WeakPtr<LocalPasswordSetupView> view_;

  ScreenExitCallback exit_callback_;

  // Keeps cryptohome authsession alive.
  std::unique_ptr<ScopedSessionRefresher> session_refresher_;

  base::WeakPtrFactory<LocalPasswordSetupScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_PASSWORD_SETUP_SCREEN_H_
