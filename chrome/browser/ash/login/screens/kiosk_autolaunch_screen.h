// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"

namespace ash {

// Representation independent class that controls screen showing auto launch
// warning to users.
class KioskAutolaunchScreen : public BaseScreen {
 public:
  enum class Result { COMPLETED, CANCELED };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  KioskAutolaunchScreen(base::WeakPtr<KioskAutolaunchScreenView> view,
                        const ScreenExitCallback& exit_callback);

  KioskAutolaunchScreen(const KioskAutolaunchScreen&) = delete;
  KioskAutolaunchScreen& operator=(const KioskAutolaunchScreen&) = delete;

  ~KioskAutolaunchScreen() override;

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

 private:
  // Called when screen is exited.
  void OnExit(bool confirmed);

  base::WeakPtr<KioskAutolaunchScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::KioskAutolaunchScreen;
}

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::KioskAutolaunchScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_
