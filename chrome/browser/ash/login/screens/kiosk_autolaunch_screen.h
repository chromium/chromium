// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

class KioskAutolaunchScreenView;

// Representation independent class that controls screen showing auto launch
// warning to users.
class KioskAutolaunchScreen : public BaseScreen {
 public:
  enum class Result { COMPLETED, CANCELED };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  KioskAutolaunchScreen(KioskAutolaunchScreenView* view,
                        const ScreenExitCallback& exit_callback);
  ~KioskAutolaunchScreen() override;

  // Called when screen is exited.
  void OnExit(bool confirmed);

  // This method is called, when view is being destroyed. Note, if Delegate
  // is destroyed earlier then it has to call SetDelegate(nullptr).
  void OnViewDestroyed(KioskAutolaunchScreenView* view);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

 private:
  KioskAutolaunchScreenView* view_;
  ScreenExitCallback exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(KioskAutolaunchScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_
