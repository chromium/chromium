// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_

#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"

class ApcExternalActionDelegate
    : public autofill_assistant::ExternalActionDelegate {
 public:
  ApcExternalActionDelegate();
  ApcExternalActionDelegate(const ApcExternalActionDelegate&) = delete;
  ApcExternalActionDelegate& operator=(const ApcExternalActionDelegate&) =
      delete;
  ~ApcExternalActionDelegate() = default;

  // ExternalActionDelegate
  void OnActionRequested(
      const autofill_assistant::external::Action& action_info,
      base::OnceCallback<void()> start_dom_checks_callback,
      base::OnceCallback<void(const autofill_assistant::external::Result&
                                  result)> end_action_callback) override;
  void OnInterruptStarted() override;
  void OnInterruptFinished() override;

 private:
  // Stores the UI state of a password change run.
  PasswordChangeRunController::Model model_;
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_
