// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"

namespace chromeos {

// Representation independent class that controls screen showing enable
// debugging screen to users.
class EnableDebuggingScreen : public BaseScreen {
 public:
  EnableDebuggingScreen(EnableDebuggingScreenView* view,
                        const base::RepeatingClosure& exit_callback);
  ~EnableDebuggingScreen() override;

  // Called by EnableDebuggingScreenHandler.
  void OnExit(bool success);
  void OnViewDestroyed(EnableDebuggingScreenView* view);

  // BaseScreen implementation:
  void Show() override;
  void Hide() override;

 protected:
  base::RepeatingClosure* exit_callback() { return &exit_callback_; }

 private:
  EnableDebuggingScreenView* view_;
  base::RepeatingClosure exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(EnableDebuggingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_
