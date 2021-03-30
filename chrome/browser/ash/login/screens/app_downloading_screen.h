// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

class AppDownloadingScreenView;

// This is App Downloading screen that tells the user the selected Android apps
// are being downloaded.
class AppDownloadingScreen : public BaseScreen {
 public:
  using TView = AppDownloadingScreenView;

  AppDownloadingScreen(AppDownloadingScreenView* view,
                       const base::RepeatingClosure& exit_callback);
  ~AppDownloadingScreen() override;

  void set_exit_callback_for_testing(base::RepeatingClosure exit_callback) {
    exit_callback_ = exit_callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  AppDownloadingScreenView* const view_;
  base::RepeatingClosure exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppDownloadingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_
