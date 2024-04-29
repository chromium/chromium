// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_osauth.mojom.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"

namespace ash {

class LocalDataLossWarningScreenView;

class LocalDataLossWarningScreen
    : public BaseOSAuthSetupScreen,
      public OobeMojoBinder<
          screens_osauth::mojom::LocalDataLossWarningPageHandler>,
      public screens_osauth::mojom::LocalDataLossWarningPageHandler {
 public:
  using TView = LocalDataLossWarningScreenView;

  enum class Result {
    kRemoveUser,
    kBackToOnlineAuth,
    kBackToLocalAuth,
    kCryptohomeError,
    kCancel,
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

  // screens_osauth::mojom::LocalDataLossWarningPageHandler:
  void OnPowerwash() override;
  void OnRecreateUser() override;
  void OnCancel() override;
  void OnBack() override;

  void OnRemovedUserDirectory(std::unique_ptr<UserContext> user_context,
                              std::optional<AuthenticationError> error);

  base::WeakPtr<LocalDataLossWarningScreenView> view_;

  ScreenExitCallback exit_callback_;

  std::unique_ptr<MountPerformer> mount_performer_;

  base::WeakPtrFactory<LocalDataLossWarningScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_LOCAL_DATA_LOSS_WARNING_SCREEN_H_
