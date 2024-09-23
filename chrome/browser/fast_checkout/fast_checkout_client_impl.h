// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "chrome/browser/fast_checkout/fast_checkout_accessibility_service.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"
#include "components/autofill/android/touch_to_fill_keyboard_suppressor.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#include "components/autofill/core/browser/ui/fast_checkout_enums.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace autofill {
class LogManager;
}

class FastCheckoutClientImpl
    : public autofill::FastCheckoutClient,
      public FastCheckoutControllerImpl::Delegate,
      public autofill::PersonalDataManagerObserver,
      public autofill::AutofillManager::Observer,
      public autofill::ContentAutofillDriverFactory::Observer,
      public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  explicit FastCheckoutClientImpl(autofill::ContentAutofillClient* client);
  ~FastCheckoutClientImpl() override;

  FastCheckoutClientImpl(const FastCheckoutClientImpl&) = delete;
  FastCheckoutClientImpl& operator=(const FastCheckoutClientImpl&) = delete;

  // FastCheckoutClient:
  bool TryToStart(
      const GURL& url,
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      base::WeakPtr<autofill::AutofillManager> autofill_manager) override;
  // The parameter `allow_further_runs` is ignored if the UI is not currently
  // showing.
  void Stop(bool allow_further_runs) override;
  bool IsRunning() const override;
  bool IsShowing() const override;
  void OnNavigation(const GURL& url, bool is_cart_or_checkout_url) override;
  autofill::FastCheckoutTriggerOutcome CanRun(
      const autofill::FormData& form,
      const autofill::FormFieldData& field,
      const autofill::AutofillManager& autofill_manager) const override;
  bool IsNotShownYet() const override;

  // FastCheckoutControllerImpl::Delegate:
  void OnOptionsSelected(
      std::unique_ptr<autofill::AutofillProfile> selected_profile,
      std::unique_ptr<autofill::CreditCard> selected_credit_card) override;
  void OnDismiss() override;

  // autofill::AutofillManager::Observer:
  void OnAfterLoadedServerPredictions(
      autofill::AutofillManager& manager) override;
  void OnAfterDidFillAutofillFormData(autofill::AutofillManager& manager,
                                      autofill::FormGlobalId form_id) override;
  void OnAutofillManagerStateChanged(
      autofill::AutofillManager& manager,
      autofill::AutofillManager::LifecycleState old_state,
      autofill::AutofillManager::LifecycleState new_state) override;

  // ContentAutofillDriverFactory::Observer:
  void OnContentAutofillDriverFactoryDestroyed(
      autofill::ContentAutofillDriverFactory& factory) override;
  void OnContentAutofillDriverCreated(
      autofill::ContentAutofillDriverFactory& factory,
      autofill::ContentAutofillDriver& driver) override;

  // autofill::payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& full_card_request,
      const autofill::CreditCard& card,
      const std::u16string& cvc) override;
  void OnFullCardRequestFailed(
      autofill::CreditCard::RecordType card_type,
      autofill::payments::FullCardRequest::FailureType failure_type) override;

  autofill::TouchToFillKeyboardSuppressor& keyboard_suppressor_for_test() {
    return keyboard_suppressor_;
  }

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

 protected:
  // Creates the UI controller.
  virtual std::unique_ptr<FastCheckoutController>
  CreateFastCheckoutController();

 private:
  friend class DISABLED_FastCheckoutClientImplTest;
  FRIEND_TEST_ALL_PREFIXES(
      DISABLED_FastCheckoutClientImplTest,
      DestroyingAutofillDriver_ResetsAutofillManagerPointer);
  FRIEND_TEST_ALL_PREFIXES(
      DISABLED_FastCheckoutClientImplTest,
      OnOptionsSelected_LocalCard_SavesFormsAndAutofillDataSelections);
  FRIEND_TEST_ALL_PREFIXES(
      DISABLED_FastCheckoutClientImplTest,
      OnOptionsSelected_ServerCard_SavesFormsAndAutofillDataSelections);
  FRIEND_TEST_ALL_PREFIXES(DISABLED_FastCheckoutClientImplTest,
                           OnAfterLoadedServerPredictions_FillsForms);
  FRIEND_TEST_ALL_PREFIXES(
      DISABLED_FastCheckoutClientImplTest,
      OnAfterDidFillAutofillFormData_SetsFillingFormsToFilledAndStops);
  FRIEND_TEST_ALL_PREFIXES(
      DISABLED_FastCheckoutClientImplTest,
      OnFullCardRequestSucceeded_InvokesCreditCardFormFill);
  FRIEND_TEST_ALL_PREFIXES(
      DISABLED_FastCheckoutClientImplTest,
      TryToFillForms_LocalCreditCard_ImmediatelyFillsCreditCardForm);

  // From autofill::PersonalDataManagerObserver.
  void OnPersonalDataChanged() override;

  // Called whenever the surface gets hidden (regardless of the cause). Informs
  // the Delegate that the surface is now hidden.
  void OnHidden();

  // Registers when a run is complete.
  void OnRunComplete(autofill::FastCheckoutRunOutcome run_outcome,
                     bool allow_further_runs = true);

  // Displays the bottom sheet UI. If the underlying autofill data is updated,
  // the method is called again to refresh the information displayed in the UI.
  void ShowFastCheckoutUI();

  // Returns the Autofill log manager if available.
  autofill::LogManager* GetAutofillLogManager() const;

  // Logs `message` to chrome://autofill-internals.
  void LogAutofillInternals(std::string message) const;

  // Populates map with forms to fill at the beginning of the run.
  void SetFormsToFill();

  // Returns `true` if all forms in `forms_to_fill_` have
  // `FillingState::kFilled`.
  bool AllFormsAreFilled() const;

  // Returns `true` if the ongoing run is in filling mode. That means if
  // `is_running_ == true`, there are unfilled `forms_to_fill_` and selections
  // of Autofill profile and credit card are present.
  bool IsFilling() const;

  // Populates `form_filling_states_` according to the forms cache of
  // `AutofillManager` and `form_signatures_to_fill_`.
  void SetFormFillingStates();

  // Returns `true` if `form` is an unfilled form of type `expected_form_type`.
  // Also sets initial filling state in `form_filling_states_`.
  bool ShouldFillForm(const autofill::FormStructure& form,
                      autofill::FormType expected_form_type) const;

  // Will be called when form extraction has been triggered in all frames.
  void OnTriggerFormExtractionFinished(bool success);

  // Tries to fill all unfilled forms cached by `autofill_manager_` if they are
  // part of the ongoing run's funnel.
  void TryToFillForms();

  // Updates filling states of forms in `forms_to_fill_` on form filled
  // notification.
  void UpdateFillingStates();

  // Makes accessibility announcements for when a form was filled.
  void A11yAnnounce(autofill::FormSignature form_signature,
                    bool is_credit_card_form);
  // Returns a pointer to the autofill profile corresponding to
  // `selected_autofill_profile_guid_`. Stops the run if it's a `nullptr`.
  const autofill::AutofillProfile* GetSelectedAutofillProfile();

  // Returns a pointer to the credit card corresponding to
  // `selected_credit_card_id_`. Stops the run if it's a `nullptr`.
  autofill::CreditCard* GetSelectedCreditCard();

  // Fills credit card form via the `autofill_manager_` and handles internal
  // state.
  void FillCreditCardForm(const autofill::FormStructure& form,
                          const autofill::FormFieldData& field,
                          const autofill::CreditCard& credit_card,
                          const std::u16string& cvc);

  // Same as Stop() but does not require `IsShowing() == true` for
  // `allow_further_runs == false` to have any effect. The `IsShowing()` guard
  // is currently required because of uncontrolled `HideFastCheckout()` calls
  // in `BrowserAutofillManager::OnHidePopupImpl()`.
  // TODO(crbug.com/40228235): remove `HideFastCheckout()` call from
  // `BrowserAutofillManger` by introducing a new `AutofillManager::Observer`
  // methods pair, then remove this method in favor of `Stop()`.
  void InternalStop(bool allow_further_runs);

  // Triggers form extraction with a delay of
  // `kSleepBetweenTriggerFormExtractionCalls`. Reparsing updates the forms
  // cache `autofill_manager_->form_structures()` with current data from the
  // renderer, eventually calling `OnAfterLoadedServerPredictions()` if there
  // were any updates. This is necessary e.g. for the case when a form has been
  // cached when it was not visible to the user and became visible in the
  // meantime.
  base::OneShotTimer form_extraction_timer_;

  // Stops the run after timeout.
  base::OneShotTimer timeout_timer_;

  // The `ChromeAutofillClient` instance attached to the same `WebContents`.
  raw_ptr<autofill::ContentAutofillClient> autofill_client_ = nullptr;

  // The `AutofillManager` instance invoking the fast checkout run. Note that
  // `this` class generally outlives `AutofillManager`.
  base::WeakPtr<autofill::AutofillManager> autofill_manager_;

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

  // Makes a11y announcements.
  std::unique_ptr<FastCheckoutAccessibilityService> accessibility_service_;

  // True if a run is ongoing; used to avoid multiple runs in parallel.
  bool is_running_ = false;

  // Autofill profile selected by the user in the bottomsheet.
  std::optional<std::string> selected_autofill_profile_guid_;

  // Credit card selected by the user in the bottomsheet.
  std::optional<std::string> selected_credit_card_id_;

  // Specifis whether the selected credit card is local or a server card.
  bool selected_credit_card_is_local_ = true;

  // The origin for which `TryToStart()` was triggered.
  url::Origin origin_;

  // Maps forms to fill during the run to their filling state.
  base::flat_map<std::pair<autofill::FormSignature, autofill::FormType>,
                 FillingState>
      form_filling_states_;

  // Signatures of forms the run intends to fill as retrieved from the
  // `FastCheckoutCapabilitiesFetcher`.
  base::flat_set<autofill::FormSignature> form_signatures_to_fill_;

  // The current state of the bottomsheet.
  autofill::FastCheckoutUIState fast_checkout_ui_state_ =
      autofill::FastCheckoutUIState::kNotShownYet;

  // Identifier of the credit card form to be filled once the CVC popup is
  // fulfilled.
  std::optional<autofill::FormGlobalId> credit_card_form_global_id_;

  // Hash of the unique run ID used for metrics.
  int64_t run_id_ = 0;

  // Suppresses the keyboard between
  // AutofillManager::Observer::On{Before,After}AskForValuesToFill() events if
  // FC may be shown.
  autofill::TouchToFillKeyboardSuppressor keyboard_suppressor_;

  base::ScopedObservation<autofill::PersonalDataManager,
                          autofill::PersonalDataManagerObserver>
      personal_data_manager_observation_{this};

  base::ScopedObservation<autofill::AutofillManager,
                          autofill::AutofillManager::Observer>
      autofill_manager_observation_{this};

  // Observes creation of ContentAutofillDrivers to inject a
  // FastCheckoutDelegateImpl into the BrowserAutofillManager.
  base::ScopedObservation<autofill::ContentAutofillDriverFactory,
                          autofill::ContentAutofillDriverFactory::Observer>
      driver_factory_observation_{this};

  // content::WebContentsUserData:
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<FastCheckoutClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CLIENT_IMPL_H_
