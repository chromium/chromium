// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_IMPL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AssistantOnboardingController;
struct AssistantOnboardingInformation;
class AssistantOnboardingPrompt;
class PrefService;

// Implementation of the `ApcOnboardingCoordinator` interface that takes care
// of onboarding/consent for automated password change.
class ApcOnboardingCoordinatorImpl : public ApcOnboardingCoordinator {
 public:
  explicit ApcOnboardingCoordinatorImpl(content::WebContents* web_contents);
  ~ApcOnboardingCoordinatorImpl() override;

  // ApcOnboardingCoordinator:
  void PerformOnboarding(Callback callback) override;
  void RevokeConsent(const std::vector<int>& description_grd_ids) override;

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
  // Helper class that listens to a `WebContents` and executes a closure to
  // open the onboarding dialog on a `DidFinishNavigation` that ends in a
  // commit.
  class DialogLauncher : public content::WebContentsObserver {
   public:
    DialogLauncher(content::WebContents* web_contents,
                   base::OnceClosure open_dialog);
    ~DialogLauncher() override;

   private:
    // content::WebContentsObserver:
    void DidFinishNavigation(content::NavigationHandle*) override;

    // The closure that opens the dialog.
    base::OnceClosure open_dialog_;
  };

  // Returns whether the user has previously accepted onboarding by checking
  // the respective pref key.
  bool IsOnboardingAlreadyAccepted();

  // Creates controller and view for the onboarding dialog and shows it.
  void OpenOnboardingDialog();

  // Handles the response from the UI controller prompting the user for consent.
  void OnControllerResponseReceived(
      bool success,
      absl::optional<int> confirmation_grd_id,
      const std::vector<int>& description_grd_ids);

  // Records that consent was given using a dialog with a confirmation button
  // with label `confirmation_grd_id` and other elements with text contents
  // `description_grd_ids`.
  void RecordConsentGiven(int confirmation_grd_id,
                          const std::vector<int>& description_grd_ids);

  // Writes `consent` to the `ConsentAuditor` instance of this profile, which
  // sends it to the backend via Chrome's sync server.
  void WriteToConsentAuditor(
      const sync_pb::UserConsentTypes::AutofillAssistantConsent& consent);

  // Returns the pref service needed to check whether onboarding was previously
  // accepted.
  PrefService* GetPrefs();

  // The `WebContents` for which onboarding is conducted.
  raw_ptr<content::WebContents> web_contents_;

  // Informs the caller about the success of the onboarding process.
  Callback callback_;

  // Controller for the dialog.
  std::unique_ptr<AssistantOnboardingController> dialog_controller_;

  // A helper object that is used to delay opening the onboarding dialog until
  // an ongoing navigation is finished.
  std::unique_ptr<DialogLauncher> dialog_launcher_;
};

#endif  // CHROME_BROWSER_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_APC_ONBOARDING_COORDINATOR_IMPL_H_
