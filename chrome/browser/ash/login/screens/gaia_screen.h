// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"

namespace ash {

// This class represents GAIA screen: login screen that is responsible for
// GAIA-based sign-in.
class GaiaScreen : public BaseScreen {
 public:
  using TView = GaiaView;

  enum class Result {
    BACK,
    CANCEL,
    ENTERPRISE_ENROLL,
    START_CONSUMER_KIOSK,
    SAML_VIDEO_TIMEOUT,
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

 private:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  base::WeakPtr<TView> view_;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::GaiaScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
