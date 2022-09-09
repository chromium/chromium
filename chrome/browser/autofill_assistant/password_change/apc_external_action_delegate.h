// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PasswordChangeRunDisplay;
class AssistantDisplayDelegate;
class ApcScrimManager;
class GURL;

namespace autofill_assistant {
struct RectF;
class WebsiteLoginManager;
}  // namespace autofill_assistant

namespace content {
class WebContents;
}  // namespace content

// Receives actions from the `HeadlessScriptController` and passes them on an
// implementation of a `PasswordChangeRunDisplay`.
// Currently `ApcExternalActionDelegate` implements two interfaces. If the
// class becomes too complex, we may later separate out the
// `PasswordChangeRunController` implementation and compose it instead.
class ApcExternalActionDelegate
    : public autofill_assistant::ExternalActionDelegate,
      public PasswordChangeRunController {
 public:
  explicit ApcExternalActionDelegate(
      content::WebContents* web_contents,
      AssistantDisplayDelegate* display_delegate,
      ApcScrimManager* apc_scrim_manager,
      autofill_assistant::WebsiteLoginManager* website_login_manager);
  ApcExternalActionDelegate(const ApcExternalActionDelegate&) = delete;
  ApcExternalActionDelegate& operator=(const ApcExternalActionDelegate&) =
      delete;
  ~ApcExternalActionDelegate() override;

  // Sets up the display to render a password change run UI,
  // needs to be called BEFORE starting a script.
  virtual void SetupDisplay();

  // ExternalActionDelegate:
  void OnActionRequested(
      const autofill_assistant::external::Action& action,
      bool is_interrupt,
      base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
      base::OnceCallback<void(const autofill_assistant::external::Result&
                                  result)> end_action_callback) override;
  void OnInterruptStarted() override;
  void OnInterruptFinished() override;

  void OnTouchableAreaChanged(
      const autofill_assistant::RectF& visual_viewport,
      const std::vector<autofill_assistant::RectF>& touchable_areas,
      const std::vector<autofill_assistant::RectF>& restricted_areas) override;

  // PasswordChangeRunController:
  void SetTopIcon(
      autofill_assistant::password_change::TopIcon top_icon) override;
  void SetTitle(const std::u16string& title) override;
  void SetDescription(const std::u16string& description) override;
  void SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep progress_step) override;
  base::WeakPtr<PasswordChangeRunController> GetWeakPtr() override;
  void ShowBasePrompt(
      const autofill_assistant::password_change::BasePromptSpecification&
          base_prompt) override;
  void OnBasePromptChoiceSelected(size_t choice_index) override;
  void ShowUseGeneratedPasswordPrompt(
      const autofill_assistant::password_change::
          UseGeneratedPasswordPromptSpecification& password_prompt,
      const std::u16string& generated_password) override;
  void OnGeneratedPasswordSelected(bool selected) override;
  void ShowStartingScreen(const GURL& url) override;
  void ShowCompletionScreen(
      base::RepeatingClosure onShowCompletionScreenDoneButtonClicked) override;
  void OpenPasswordManager() override;
  void ShowErrorScreen() override;
  bool PasswordWasSuccessfullyChanged() override;

 private:
  friend class ApcExternalActionDelegateTest;

  // PasswordChangeRunController:
  void Show(base::WeakPtr<PasswordChangeRunDisplay> password_change_run_display)
      override;

  // Ends the current action by notifying the `ExternalActionController` about
  // the `success` of the action. If non-empty, `action_result` is passed
  // as the result payload. Otherwise, no payload is set.
  void EndAction(bool success,
                 absl::optional<autofill_assistant::password_change::
                                    GenericPasswordChangeSpecificationResult>
                     action_result = absl::nullopt);

  // Handler methods for the different actions that `ApcExternalActionDelegate`
  // supports.
  void HandleBasePrompt(
      const autofill_assistant::password_change::BasePromptSpecification&
          specification);
  void HandleGeneratedPasswordPrompt(
      const autofill_assistant::password_change::
          UseGeneratedPasswordPromptSpecification& specification);
  void HandleUpdateSidePanel(
      const autofill_assistant::password_change::UpdateSidePanelSpecification&
          specification);

  void OnBasePromptDomUpdateReceived(
      const autofill_assistant::external::ElementConditionsUpdate& update);

  // The `WebContents` on which the run is performed.
  const raw_ptr<content::WebContents> web_contents_;

  // The callback that terminates the current action.
  base::OnceCallback<void(const autofill_assistant::external::Result& result)>
      end_action_callback_;

  // The callback that starts regular DOM checks.
  base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback_;

  // Indicates whether a base prompt should send back a result payload.
  bool base_prompt_should_send_payload_ = false;

  // Stores the UI state of a password change run.
  PasswordChangeRunController::Model model_;

  // Back up for the state before the start of an interrupt.
  absl::optional<PasswordChangeRunController::Model> model_before_interrupt_;

  // The return values associated with each currently shown base prompt choice.
  // It is empty when no prompt is being displayed.
  std::vector<std::string> base_prompt_return_values_;

  // The view that renders a password change run flow.
  base::WeakPtr<PasswordChangeRunDisplay> password_change_run_display_ =
      nullptr;

  // The display where we render the UI for a password change run.
  raw_ptr<AssistantDisplayDelegate> display_delegate_ = nullptr;

  // The scrim manager to update the overlay and html elements showcasting.
  raw_ptr<ApcScrimManager> apc_scrim_manager_ = nullptr;

  // Use to handle interactions with the password manager.
  raw_ptr<autofill_assistant::WebsiteLoginManager> website_login_manager_ =
      nullptr;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<PasswordChangeRunController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_EXTERNAL_ACTION_DELEGATE_H_
