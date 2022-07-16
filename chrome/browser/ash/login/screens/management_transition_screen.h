// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MANAGEMENT_TRANSITION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MANAGEMENT_TRANSITION_SCREEN_H_

#include "base/callback.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/management_transition_screen_handler.h"

namespace ash {

class ManagementTransitionScreen : public BaseScreen {
 public:
  ManagementTransitionScreen(ManagementTransitionScreenView* view,
                             const base::RepeatingClosure& exit_callback);

  ManagementTransitionScreen(const ManagementTransitionScreen&) = delete;
  ManagementTransitionScreen& operator=(const ManagementTransitionScreen&) =
      delete;

  ~ManagementTransitionScreen() override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(ManagementTransitionScreenView* view_);

  // Called when transition has finished, exits the screen.
  void OnManagementTransitionFinished();

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  base::RepeatingClosure* exit_callback() { return &exit_callback_; }

 private:
  ManagementTransitionScreenView* view_;
  base::RepeatingClosure exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::ManagementTransitionScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MANAGEMENT_TRANSITION_SCREEN_H_
