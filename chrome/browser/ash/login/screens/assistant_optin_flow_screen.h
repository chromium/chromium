// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_

#include <string>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class AssistantOptInFlowScreenView;

class AssistantOptInFlowScreen : public BaseScreen {
 public:
  using TView = AssistantOptInFlowScreenView;

  enum class Result { NEXT, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  AssistantOptInFlowScreen(base::WeakPtr<AssistantOptInFlowScreenView> view,
                           const ScreenExitCallback& exit_callback);

  AssistantOptInFlowScreen(const AssistantOptInFlowScreen&) = delete;
  AssistantOptInFlowScreen& operator=(const AssistantOptInFlowScreen&) = delete;

  ~AssistantOptInFlowScreen() override;

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
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override {}
  void OnUserAction(const base::Value::List& args) override;

 private:
  base::WeakPtr<AssistantOptInFlowScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ASSISTANT_OPTIN_FLOW_SCREEN_H_
