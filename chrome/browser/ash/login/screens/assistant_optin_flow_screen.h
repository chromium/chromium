// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_

#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

class AssistantOptInFlowScreenView;

class AssistantOptInFlowScreen : public BaseScreen {
 public:
  using TView = AssistantOptInFlowScreenView;

  enum class Result { NEXT, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  AssistantOptInFlowScreen(AssistantOptInFlowScreenView* view,
                           const ScreenExitCallback& exit_callback);
  ~AssistantOptInFlowScreen() override;

  // Called when view is destroyed so there's no dead reference to it.
  void OnViewDestroyed(AssistantOptInFlowScreenView* view_);

  static std::unique_ptr<base::AutoReset<bool>>
  ForceLibAssistantEnabledForTesting(bool enabled);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  AssistantOptInFlowScreenView* view_;
  ScreenExitCallback exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInFlowScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_
