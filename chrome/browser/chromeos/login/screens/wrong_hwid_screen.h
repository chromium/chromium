// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class WrongHWIDScreenView;

// Representation independent class that controls screen showing warning about
// malformed HWID to users.
class WrongHWIDScreen : public BaseScreen {
 public:
  WrongHWIDScreen(WrongHWIDScreenView* view,
                  const base::RepeatingClosure& exit_callback);
  ~WrongHWIDScreen() override;

  // Called when screen is exited.
  void OnExit();
  // This method is called, when view is being destroyed. Note, if Delegate
  // is destroyed earlier then it has to call SetDelegate(NULL).
  void OnViewDestroyed(WrongHWIDScreenView* view);

  // BaseScreen implementation:
  void Show() override;
  void Hide() override;

 private:
  WrongHWIDScreenView* view_;
  base::RepeatingClosure exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(WrongHWIDScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_
