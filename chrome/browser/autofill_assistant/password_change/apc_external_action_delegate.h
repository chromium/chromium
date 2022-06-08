// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"

class PasswordChangeRunDisplay;
class AssistantDisplayDelegate;

class ApcExternalActionDelegate
    : public autofill_assistant::ExternalActionDelegate,
      public PasswordChangeRunController {
 public:
  explicit ApcExternalActionDelegate(
      AssistantDisplayDelegate* display_delegate);
  ApcExternalActionDelegate(const ApcExternalActionDelegate&) = delete;
  ApcExternalActionDelegate& operator=(const ApcExternalActionDelegate&) =
      delete;
  ~ApcExternalActionDelegate() override;

  // Sets up the display to render a password change run UI,
  // needs to be called BEFORE starting a script.
  void SetupDisplay();

  // ExternalActionDelegate
  void OnActionRequested(
      const autofill_assistant::external::Action& action_info,
      base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
      base::OnceCallback<void(const autofill_assistant::external::Result&
                                  result)> end_action_callback) override;
  void OnInterruptStarted() override;
  void OnInterruptFinished() override;

  // PasswordChangeRunController
  void SetTopIcon(
      autofill_assistant::password_change::TopIcon top_icon) override;
  void SetTitle(const std::u16string& title) override;
  void SetDescription(const std::u16string& description) override;
  void SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep progress_step) override;
  base::WeakPtr<PasswordChangeRunController> GetWeakPtr() override;
  void ShowBasePrompt(const autofill_assistant::password_change::BasePrompt&
                          base_prompt) override;
  void OnBasePromptOptionSelected(int option_index) override;
  void ShowSuggestedPasswordPrompt(
      const std::u16string& suggested_password) override;
  void OnSuggestedPasswordSelected(bool selected) override;

 private:
  // PasswordChangeRunController
  void Show(base::WeakPtr<PasswordChangeRunDisplay> password_change_run_display)
      override;
  // Stores the UI state of a password change run.
  PasswordChangeRunController::Model model_;

  // The view that renders a password change run flow.
  base::WeakPtr<PasswordChangeRunDisplay> password_change_run_display_ =
      nullptr;

  // The display where we render the UI for a password change run.
  raw_ptr<AssistantDisplayDelegate> display_delegate_ = nullptr;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<PasswordChangeRunController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_
