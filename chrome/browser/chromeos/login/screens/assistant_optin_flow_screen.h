// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace chromeos {

class AssistantOptInFlowScreenView;

class AssistantOptInFlowScreen : public BaseScreen {
 public:
  AssistantOptInFlowScreen(AssistantOptInFlowScreenView* view,
                           const base::RepeatingClosure& exit_callback);
  ~AssistantOptInFlowScreen() override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(AssistantOptInFlowScreenView* view_);

  // BaseScreen:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

  void SetSkipForTesting() { skip_for_testing_ = true; }

 private:
  AssistantOptInFlowScreenView* view_;
  base::RepeatingClosure exit_callback_;

  // Skip the screen for testing if set to true.
  bool skip_for_testing_ = false;

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInFlowScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_
