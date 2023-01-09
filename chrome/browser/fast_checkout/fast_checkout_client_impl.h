// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_

#include "base/scoped_observation.h"
#include "chrome/browser/fast_checkout/fast_checkout_client.h"
#include "chrome/browser/fast_checkout/fast_checkout_enums.h"
#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace autofill {
class LogManager;
}

constexpr char kUmaKeyFastCheckoutRunOutcome[] =
    "Autofill.FastCheckout.RunOutcome";

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
  bool TryToStart(const GURL& url,
                  const autofill::FormData& form,
                  const autofill::FormFieldData& field,
                  autofill::AutofillDriver* autofill_driver) override;
  void Stop(bool allow_further_runs) override;
  bool IsRunning() const override;
  bool IsShowing() const override;

  // FastCheckoutControllerImpl::Delegate:
  void OnOptionsSelected(
      std::unique_ptr<autofill::AutofillProfile> selected_profile,
      std::unique_ptr<autofill::CreditCard> selected_credit_card) override;
  void OnDismiss() override;

#if defined(UNIT_TEST)
  void set_trigger_validator_for_test(
      std::unique_ptr<FastCheckoutTriggerValidator> trigger_validator) {
    trigger_validator_ = std::move(trigger_validator);
  }

  void set_autofill_client_for_test(autofill::AutofillClient* autofill_client) {
    autofill_client_ = autofill_client;
  }

  autofill::ContentAutofillDriver* get_autofill_driver_for_test() {
    return autofill_driver_;
  }
#endif

 protected:
  explicit FastCheckoutClientImpl(content::WebContents* web_contents);

  // Creates the UI controller.
  virtual std::unique_ptr<FastCheckoutController>
  CreateFastCheckoutController();

 private:
  friend class content::WebContentsUserData<FastCheckoutClientImpl>;

  // From autofill::PersonalDataManagerObserver.
  void OnPersonalDataChanged() override;

  // Called whenever the surface gets hidden (regardless of the cause). Informs
  // the Delegate that the surface is now hidden.
  void OnHidden();

  // Registers when a run is complete. Used in callbacks.
  void OnRunComplete();

  // Displays the bottom sheet UI. If the underlying autofill data is updated,
  // the method is called again to refresh the information displayed in the UI.
  void ShowFastCheckoutUI();

  // Turns keyboard suppression on and off.
  void SetShouldSuppressKeyboard(bool suppress);

  // Returns the Autofill log manager if available.
  autofill::LogManager* GetAutofillLogManager() const;

  // Logs `message` to chrome://autofill-internals.
  void LogAutofillInternals(std::string message) const;

  // The `ChromeAutofillClient` instanced attached to the same `WebContents`.
  raw_ptr<autofill::AutofillClient> autofill_client_ = nullptr;

  // The `ContentAutofillDriver` instance invoking the fast checkout run. This
  // class generally outlives `autofill_driver_` so extra care needs to be taken
  // with this pointer. It gets reset in `Stop(..)` which is (also) called from
  // `~BrowserAutofillManager()` when the `ContentAutofillDriver` instance gets
  // destroyed.
  raw_ptr<autofill::ContentAutofillDriver> autofill_driver_ = nullptr;

  // Fast Checkout UI Controller. Responsible for showing the bottomsheet and
  // handling user selections.
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;

  // Helper class providing information about address profiles and credit cards.
  std::unique_ptr<FastCheckoutPersonalDataHelper> personal_data_helper_;

  // Checks whether a run should be permitted or not.
  std::unique_ptr<FastCheckoutTriggerValidator> trigger_validator_;

  // True if a run is ongoing; used to avoid multiple runs in parallel.
  bool is_running_ = false;

  // The url for which `Start()` was triggered.
  GURL url_;

  // The current state of the bottomsheet.
  FastCheckoutUIState fast_checkout_ui_state_ =
      FastCheckoutUIState::kNotShownYet;

  base::ScopedObservation<autofill::PersonalDataManager,
                          autofill::PersonalDataManagerObserver>
      personal_data_manager_observation_{this};

  // content::WebContentsUserData:
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
