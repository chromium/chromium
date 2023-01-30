// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_

#include "base/scoped_observation.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_client.h"
#include "chrome/browser/fast_checkout/fast_checkout_enums.h"
#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"
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
  bool TryToStart(
      const GURL& url,
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      base::WeakPtr<autofill::AutofillManager> autofill_manager) override;
  void Stop(bool allow_further_runs) override;
  bool IsRunning() const override;
  bool IsShowing() const override;

  // FastCheckoutControllerImpl::Delegate:
  void OnOptionsSelected(
      std::unique_ptr<autofill::AutofillProfile> selected_profile,
      std::unique_ptr<autofill::CreditCard> selected_credit_card) override;
  void OnDismiss() override;

  // Filling state of a form during a run.
  enum class FillingState {
    // Form was not attempted to be filled.
    kNotFilled = 0,

    // Autofill was invoked on the form but this clas was not notified about the
    // form being filled.
    kFilling = 1,

    // This form has been filled.
    kFilled = 2
  };

#if defined(UNIT_TEST)
  void set_trigger_validator_for_test(
      std::unique_ptr<FastCheckoutTriggerValidator> trigger_validator) {
    trigger_validator_ = std::move(trigger_validator);
  }

  void set_autofill_client_for_test(autofill::AutofillClient* autofill_client) {
    autofill_client_ = autofill_client;
  }

  base::WeakPtr<autofill::AutofillManager> get_autofill_manager_for_test() {
    return autofill_manager_;
  }

  autofill::AutofillProfile* get_autofill_profile_for_test() {
    return selected_autofill_profile_.get();
  }

  autofill::CreditCard* get_credit_card_for_test() {
    return selected_credit_card_.get();
  }

  const base::flat_map<autofill::FormSignature, FillingState>&
  get_forms_to_fill_for_test() {
    return forms_to_fill_;
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

  // Populates map with forms to fill at the beginning of the run.
  void SetFormsToFill();

  // The `ChromeAutofillClient` instanced attached to the same `WebContents`.
  raw_ptr<autofill::AutofillClient> autofill_client_ = nullptr;

  // The `AutofillManager` instance invoking the fast checkout run. Note that
  // `this` class generally outlives `AutofillManager`.
  base::WeakPtr<autofill::AutofillManager> autofill_manager_ = nullptr;

  // Weak reference to the `FastCheckoutCapabilitiesFetcher` instance attached
  // to `this` web content's browser context.
  raw_ptr<FastCheckoutCapabilitiesFetcher> fetcher_ = nullptr;

  // Fast Checkout UI Controller. Responsible for showing the bottomsheet and
  // handling user selections.
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;

  // Helper class providing information about address profiles and credit cards.
  std::unique_ptr<FastCheckoutPersonalDataHelper> personal_data_helper_;

  // Checks whether a run should be permitted or not.
  std::unique_ptr<FastCheckoutTriggerValidator> trigger_validator_;

  // True if a run is ongoing; used to avoid multiple runs in parallel.
  bool is_running_ = false;

  // Autofill profile selected by the user in the bottomsheet.
  std::unique_ptr<autofill::AutofillProfile> selected_autofill_profile_;

  // Credit card selected by the user in the bottomsheet.
  std::unique_ptr<autofill::CreditCard> selected_credit_card_;

  // The origin for which `TryToStart()` was triggered.
  url::Origin origin_;

  // Maps forms to fill during the run to their filling state.
  base::flat_map<autofill::FormSignature, FillingState> forms_to_fill_;

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
