// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_IMPL_H_

#include "chrome/browser/autofill_assistant/password_change/apc_client.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_side_panel_coordinator.h"
#include "components/autofill_assistant/browser/public/external_script_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class ApcExternalActionDelegate;

// TODO(crbug.com/1322419): Observe the SidePanel so that we can destruct
// Onboarding, ScriptExecution, etc. on close.

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
  bool Start(const GURL& url,
             const std::string& username,
             bool skip_login) override;
  void Stop() override;
  bool IsRunning() const override;

 protected:
  // Creates an onboarding coordinator. Protected to allow for overrides
  // by test classes.
  virtual std::unique_ptr<ApcOnboardingCoordinator>
  CreateOnboardingCoordinator();

  // Creates an external script controller. Protected to allow for overrides
  // by test classes.
  virtual std::unique_ptr<autofill_assistant::ExternalScriptController>
  CreateExternalScriptController();

  explicit ApcClientImpl(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<ApcClientImpl>;

  // Registers whether onboarding was successful or not (i.e. whether consent
  // has been given). Used in callbacks.
  void OnOnboardingComplete(bool success);

  // Registers when a run is complete. Used in callbacks.
  void OnRunComplete(
      autofill_assistant::ExternalScriptController::ScriptResult result);

  // AssistantSidePanelCoordinator::Observer:
  void OnHidden() override;

  // The delegate is responsible for handling protos received from backend DSL
  // actions and UI updates.
  std::unique_ptr<ApcExternalActionDelegate> apc_external_action_delegate_;

  // Controls a script run triggered by the headless API. This class is
  // responsible for handling the forwarding of action to
  // `apc_external_action_delegate_` and managing the run lifetime.
  std::unique_ptr<autofill_assistant::ExternalScriptController>
      external_script_controller_;

  // The username for which `Start()` was triggered.
  std::string username_;

  // The url for which `Start()` was triggered.
  GURL url_;

  // Whether the login step of a script run should be skipped.
  // This is used during triggers from the leak warning.
  bool skip_login_;

  // The state of the `ApcClient` to avoid that a run is started while
  // another is already ongoing in the tab.
  bool is_running_ = false;

  // Orchestrates prompting the user for consent if it has not been given
  // previously.
  std::unique_ptr<ApcOnboardingCoordinator> onboarding_coordinator_;

  // The coordinator for the side panel.
  std::unique_ptr<AssistantSidePanelCoordinator> side_panel_coordinator_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_CLIENT_IMPL_H_
