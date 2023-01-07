// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_IMPL_H_

#include "chrome/browser/autofill_assistant/password_change/apc_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_side_panel_coordinator.h"
#include "components/autofill_assistant/browser/public/headless_script_controller.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class ApcExternalActionDelegate;
class ApcScrimManager;
class AssistantStoppedBubbleCoordinator;

namespace autofill_assistant {
class WebsiteLoginManager;
}  // namespace autofill_assistant

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

// Implementation of the ApcClient interface that attaches itself to a
// `WebContents`.
class ApcClientImpl : public content::WebContentsUserData<ApcClientImpl>,
                      public ApcClient,
                      public AssistantSidePanelCoordinator::Observer {
 public:
  ~ApcClientImpl() override;

  ApcClientImpl(const ApcClientImpl&) = delete;
  ApcClientImpl& operator=(const ApcClientImpl&) = delete;

  // ApcClient:
  void Start(
      const GURL& url,
      const std::string& username,
      bool skip_login,
      ResultCallback callback,
      absl::optional<DebugRunInformation> debug_run_information) override;
  void Stop(bool success) override;
  bool IsRunning() const override;
  void PromptForConsent(OnboardingResultCallback callback) override;
  void RevokeConsent(const std::vector<int>& description_grd_ids) override;

 protected:
  // The following protected methods are factory functions that may be
  // overridden in tests.

  // Creates an assistant stopped bubble coordinator.
  virtual std::unique_ptr<AssistantStoppedBubbleCoordinator>
  CreateAssistantStoppedBubbleCoordinator();

  // Creates an onboarding coordinator.
  virtual std::unique_ptr<ApcOnboardingCoordinator>
  CreateOnboardingCoordinator();

  // Creates a side panel coordinator.
  virtual std::unique_ptr<AssistantSidePanelCoordinator> CreateSidePanel();

  // Creates an external script controller.
  virtual std::unique_ptr<autofill_assistant::HeadlessScriptController>
  CreateHeadlessScriptController();

  // Gets the RunTimeManager used to disable dialogs and prompts, such as
  // password manager, translation dialogs and permissions. Protected to allow
  // for overrides by test classes.
  virtual autofill_assistant::RuntimeManager* GetRuntimeManager();

  // Creates the ApcScrimManager used to apply a scrim over the webcontent
  // during script runs.
  virtual std::unique_ptr<ApcScrimManager> CreateApcScrimManager();

  // Creates the external action delegate responsible for receiving and handling
  // action protos.
  virtual std::unique_ptr<ApcExternalActionDelegate>
  CreateApcExternalActionDelegate();

  // Creates the website login manager to handle interactions with the password
  // manager.
  virtual std::unique_ptr<autofill_assistant::WebsiteLoginManager>
  CreateWebsiteLoginManager();

  // Get the `PasswordManagerClient` so that we can initialize
  // `website_login_manager_`.
  virtual password_manager::PasswordManagerClient* GetPasswordManagerClient();

  explicit ApcClientImpl(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<ApcClientImpl>;

  // Returns a map of script parameters that used to start an Autofill Assistant
  // flow.
  base::flat_map<std::string, std::string> GetScriptParameters() const;

  // Registers whether onboarding was successful or not (i.e. whether consent
  // has been given). Used in callbacks.
  void OnOnboardingComplete(bool success);

  // Registers when a run is complete. Used in callbacks.
  void OnRunComplete(
      autofill_assistant::HeadlessScriptController::ScriptResult result);

  // AssistantSidePanelCoordinator::Observer
  void OnHidden() override;

  void CloseSidePanel();

  // The delegate is responsible for handling protos received from backend DSL
  // actions and UI updates.
  std::unique_ptr<ApcExternalActionDelegate> apc_external_action_delegate_;

  // Controls a script run triggered by the headless API. This class is
  // responsible for handling the forwarding of action to
  // `apc_external_action_delegate_` and managing the run lifetime.
  std::unique_ptr<autofill_assistant::HeadlessScriptController>
      external_script_controller_;

  // The username for which `Start()` was triggered.
  std::string username_;

  // The url for which `Start()` was triggered.
  GURL url_;

  // Whether the login step of a script run should be skipped.
  // This is used during triggers from the leak warning.
  bool skip_login_;

  // If set, contains the parameters for a debug run.
  absl::optional<DebugRunInformation> debug_run_information_;

  // The state of the `ApcClient` to avoid that a run is started while
  // another is already ongoing in the tab.
  bool is_running_ = false;

  // The callback that signals the end of the run.
  ResultCallback result_callback_;

  // Orchestrates prompting the user for consent if it has not been given
  // previously.
  std::unique_ptr<ApcOnboardingCoordinator> onboarding_coordinator_;

  // The coordinator for the side panel.
  std::unique_ptr<AssistantSidePanelCoordinator> side_panel_coordinator_;

  // Manages the scrim shown during a password change run.
  std::unique_ptr<ApcScrimManager> scrim_manager_;

  // Bubble that is shown when a flow ends without script completion.
  std::unique_ptr<AssistantStoppedBubbleCoordinator>
      assistant_stopped_bubble_coordinator_;

  // The website login manager used to handle iteractions with the password
  // manager.
  std::unique_ptr<autofill_assistant::WebsiteLoginManager>
      website_login_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_IMPL_H_
