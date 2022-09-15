// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_

#include "base/scoped_observation.h"
#include "chrome/browser/fast_checkout/fast_checkout_client.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/public/headless_script_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class FastCheckoutExternalActionDelegate;

class FastCheckoutClientImpl
    : public content::WebContentsUserData<FastCheckoutClientImpl>,
      public FastCheckoutClient,
      public FastCheckoutControllerImpl::Delegate,
      public autofill::PersonalDataManagerObserver {
 public:
  ~FastCheckoutClientImpl() override;

  FastCheckoutClientImpl(const FastCheckoutClientImpl&) = delete;
  FastCheckoutClientImpl& operator=(const FastCheckoutClientImpl&) = delete;

  // FastCheckoutClient:
  bool Start(base::WeakPtr<autofill::FastCheckoutDelegate> delegate,
             const GURL& url) override;
  void Stop() override;
  bool IsRunning() const override;

  // FastCheckoutControllerImpl::Delegate:
  void OnOptionsSelected(
      std::unique_ptr<autofill::AutofillProfile> selected_profile,
      std::unique_ptr<autofill::CreditCard> selected_credit_card) override;
  void OnDismiss() override;

 protected:
  explicit FastCheckoutClientImpl(content::WebContents* web_contents);

  // Creates the headless script controller.
  virtual std::unique_ptr<autofill_assistant::HeadlessScriptController>
  CreateHeadlessScriptController();

  // Creates the external action delegate.
  virtual std::unique_ptr<FastCheckoutExternalActionDelegate>
  CreateFastCheckoutExternalActionDelegate();

  // Creates the UI controller.
  virtual std::unique_ptr<FastCheckoutController>
  CreateFastCheckoutController();

 private:
  friend class content::WebContentsUserData<FastCheckoutClientImpl>;

  // From autofill::PersonalDataManagerObserver.
  void OnPersonalDataChanged() override;

  // Returns the current active personal data manager.
  autofill::PersonalDataManager* GetPersonalDataManager();

  // Called whenever the surface gets hidden (regardless of the cause). Informs
  // the Delegate that the surface is now hidden.
  void OnHidden();

  // Registers when a run is complete. Used in callbacks.
  void OnRunComplete(
      autofill_assistant::HeadlessScriptController::ScriptResult result);

  // Displays the bottom sheet UI. If the underlying autofill data is updated,
  // the method is called again to refresh the information displayed in the UI.
  void ShowFastCheckoutUI();

  // Turns keyboard suppression on and off.
  void SetShouldSuppressKeyboard(bool suppress);

  // Registers when onboarding was completed successfully and the scripts
  // are ready to run.
  void OnOnboardingCompletedSuccessfully();

  // Delegate for the surface being shown.
  base::WeakPtr<autofill::FastCheckoutDelegate> delegate_;

  // The delegate is responsible for handling protos received from backend DSL
  // actions.
  std::unique_ptr<FastCheckoutExternalActionDelegate>
      fast_checkout_external_action_delegate_;

  // Controls a script run triggered by the headless API. This class is
  // responsible for handling the forwarding of actions to
  // `apc_external_action_delegate_` and managing the run lifetime.
  std::unique_ptr<autofill_assistant::HeadlessScriptController>
      external_script_controller_;

  // Fast Checkout UI Controller. Responsible for showing the bottomsheet and
  // handling user selections.
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;

  // True if a run is ongoing; used to avoid multiple runs in parallel.
  bool is_running_ = false;

  // The url for which `Start()` was triggered.
  GURL url_;

  base::ScopedObservation<autofill::PersonalDataManager,
                          autofill::PersonalDataManagerObserver>
      personal_data_manager_observation_{this};

  // content::WebContentsUserData:
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
