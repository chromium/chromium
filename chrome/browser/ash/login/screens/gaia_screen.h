// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

class GaiaView;

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
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  explicit GaiaScreen(const ScreenExitCallback& exit_callback);
  ~GaiaScreen() override;

  void SetView(GaiaView* view);

  // Loads online Gaia into the webview.
  void LoadOnline(const AccountId& account);
  // Loads online Gaia (for child signup) into the webview.
  void LoadOnlineForChildSignup();
  // Loads online Gaia (for child signin) into the webview.
  void LoadOnlineForChildSignin();

 private:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;
  bool HandleAccelerator(ash::LoginAcceleratorAction action) override;

  GaiaView* view_ = nullptr;

  ScreenExitCallback exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(GaiaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_SCREEN_H_
