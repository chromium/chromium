// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"

namespace ash {

class LocalDataLossWarningScreenView;

class LocalDataLossWarningScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = LocalDataLossWarningScreenView;

  enum class Result {
    kRemoveUser,
    kBack,
    kCryptohomeError,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  LocalDataLossWarningScreen(base::WeakPtr<LocalDataLossWarningScreenView> view,
                             const ScreenExitCallback& exit_callback);

  LocalDataLossWarningScreen(const LocalDataLossWarningScreen&) = delete;
  LocalDataLossWarningScreen& operator=(const LocalDataLossWarningScreen&) =
      delete;

  ~LocalDataLossWarningScreen() override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void OnRemovedUserDirectory(std::unique_ptr<UserContext> user_context,
                              absl::optional<AuthenticationError> error);

  base::WeakPtr<LocalDataLossWarningScreenView> view_;

  ScreenExitCallback exit_callback_;

  std::unique_ptr<MountPerformer> mount_performer_;

  base::WeakPtrFactory<LocalDataLossWarningScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_H_
