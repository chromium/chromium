// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_IMPL_H_

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"

namespace content {
class WebContents;
}  // namespace content

class PrefService;

// Implementation of the `ApcOnboardingCoordinator` interface that takes care
// of onboarding/consent for automated password change.
class ApcOnboardingCoordinatorImpl : public ApcOnboardingCoordinator {
 public:
  explicit ApcOnboardingCoordinatorImpl(content::WebContents* web_contents);
  ~ApcOnboardingCoordinatorImpl() override;

  // ApcOnboardingCoordinator:
  void PerformOnboarding(Callback callback) override;

 protected:
  // These methods pass through their arguments to the respective factory
  // functions. Encapsulating them allows injecting mock controllers and
  // mock prompts during unit tests.
  virtual std::unique_ptr<AssistantOnboardingController>
  CreateOnboardingController(
      const AssistantOnboardingInformation& onboarding_information);
  virtual base::WeakPtr<AssistantOnboardingPrompt> CreateOnboardingPrompt(
      base::WeakPtr<AssistantOnboardingController> controller);

 private:
  // Returns whether the user has previously accepted onboarding by checking
  // the respective pref key.
  bool IsOnboardingAlreadyAccepted();

  // Handles the response from the UI controller prompting the user for consent.
  void OnControllerResponseReceived(bool success);

  // Returns the pref service needed to check whether onboarding was previously
  // accepted.
  PrefService* GetPrefs();

  // The `WebContents` for which onboarding is conducted.
  raw_ptr<content::WebContents> web_contents_;

  // Informs the caller about the success of the onboarding process.
  Callback callback_;

  // Controller for the dialog.
  std::unique_ptr<AssistantOnboardingController> dialog_controller_;
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_IMPL_H_
