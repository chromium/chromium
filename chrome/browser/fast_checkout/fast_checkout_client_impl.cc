// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"
#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/fast_checkout/fast_checkout_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/fast_checkout_delegate.h"
#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"
#include "components/autofill_assistant/browser/public/public_script_parameters.h"
#include "content/public/browser/web_contents_user_data.h"

namespace {
constexpr char kIntentValue[] = "CHROME_FAST_CHECKOUT";
constexpr char kTrue[] = "true";
// TODO(crbug.com/1338521): Define and specify proper caller(s) and source(s).
constexpr char kCaller[] = "7";  // run was started from within Chromium
constexpr char kSource[] = "1";  // run was started organically

std::vector<autofill::CreditCard*> GetValidCreditCards(
    autofill::PersonalDataManager* pdm) {
  // TODO(crbug.com/1334642): Check on autofill_client whether server credit
  // cards are supported.
  std::vector<autofill::CreditCard*> cards = pdm->GetCreditCardsToSuggest(true);
  base::EraseIf(cards,
                base::not_fn(&autofill::CreditCard::IsCompleteValidCard));
  return cards;
}

std::vector<autofill::AutofillProfile*> GetValidAddressProfiles(
    autofill::PersonalDataManager* pdm) {
  // Trigger only if there is at least 1 complete address profile on file.
  std::vector<autofill::AutofillProfile*> profiles =
      pdm->GetProfilesToSuggest();

  base::EraseIf(profiles, [&pdm](const autofill::AutofillProfile* profile) {
    return !fast_checkout::IsCompleteAddressProfile(profile, pdm->app_locale());
  });
  return profiles;
}

}  // namespace

FastCheckoutClientImpl::FastCheckoutClientImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<FastCheckoutClientImpl>(*web_contents) {}

FastCheckoutClientImpl::~FastCheckoutClientImpl() = default;

bool FastCheckoutClientImpl::Start(
    base::WeakPtr<autofill::FastCheckoutDelegate> delegate,
    const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kFastCheckout))
    return false;

  if (is_running_)
    return false;

  autofill::PersonalDataManager* pdm = GetPersonalDataManager();
  DCHECK(pdm);
  // Trigger only if there is at least 1 valid Autofill profile on file.
  if (GetValidAddressProfiles(pdm).empty()) {
    base::UmaHistogramEnumeration(
        autofill::kUmaKeyFastCheckoutTriggerOutcome,
        autofill::FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile);
    return false;
  }
  // Trigger only if there is at least 1 complete valid credit card on file.
  if (GetValidCreditCards(pdm).empty()) {
    base::UmaHistogramEnumeration(
        autofill::kUmaKeyFastCheckoutTriggerOutcome,
        autofill::FastCheckoutTriggerOutcome::kFailureNoValidCreditCard);
    return false;
  }

  is_running_ = true;
  url_ = url;
  delegate_ = std::move(delegate);
  personal_data_manager_observation_.Observe(GetPersonalDataManager());

  base::flat_map<std::string, std::string> params_map{
      {autofill_assistant::public_script_parameters::kIntentParameterName,
       kIntentValue},
      {autofill_assistant::public_script_parameters::
           kOriginalDeeplinkParameterName,
       url_.spec()},
      {autofill_assistant::public_script_parameters::kEnabledParameterName,
       kTrue},
      {autofill_assistant::public_script_parameters::
           kStartImmediatelyParameterName,
       kTrue},
      {autofill_assistant::public_script_parameters::kCallerParameterName,
       kCaller},
      {autofill_assistant::public_script_parameters::kSourceParameterName,
       kSource},
  };

  fast_checkout_external_action_delegate_ =
      CreateFastCheckoutExternalActionDelegate();
  external_script_controller_ = CreateHeadlessScriptController();

  SetShouldSuppressKeyboard(true);

  external_script_controller_->StartScript(
      params_map,
      base::BindOnce(&FastCheckoutClientImpl::OnRunComplete,
                     base::Unretained(this)),
      /*use_autofill_assistant_onboarding=*/true,
      base::BindOnce(&FastCheckoutClientImpl::OnOnboardingCompletedSuccessfully,
                     base::Unretained(this)),
      /*suppress_browsing_features=*/false);

  return true;
}

void FastCheckoutClientImpl::OnOnboardingCompletedSuccessfully() {
  fast_checkout_controller_ = CreateFastCheckoutController();
  ShowFastCheckoutUI();
}

void FastCheckoutClientImpl::ShowFastCheckoutUI() {
  autofill::PersonalDataManager* pdm = GetPersonalDataManager();

  std::vector<autofill::AutofillProfile*> profiles_to_suggest =
      pdm->GetProfilesToSuggest();

  std::vector<autofill::CreditCard*> cards_to_suggest =
      pdm->GetCreditCardsToSuggest(true);
  // Do not offer cards with empty number.
  base::EraseIf(cards_to_suggest, [](const autofill::CreditCard* card) {
    return card->GetRawInfo(autofill::CREDIT_CARD_NUMBER).empty();
  });

  fast_checkout_controller_->Show(profiles_to_suggest, cards_to_suggest);
}

void FastCheckoutClientImpl::SetShouldSuppressKeyboard(bool suppress) {
  if (delegate_) {
    autofill::ContentAutofillDriver* driver =
        static_cast<autofill::ContentAutofillDriver*>(delegate_->GetDriver());
    if (driver) {
      driver->SetShouldSuppressKeyboard(suppress);
    }
  }
}

void FastCheckoutClientImpl::OnRunComplete(
    autofill_assistant::HeadlessScriptController::ScriptResult result) {
  // TODO(crbug.com/1338522): Handle failed result.
  Stop();
}

void FastCheckoutClientImpl::Stop() {
  external_script_controller_.reset();
  fast_checkout_controller_.reset();
  is_running_ = false;
  personal_data_manager_observation_.Reset();

  // `OnHidden` is not called if the bottom sheet never managed to show,
  // e.g. due to a failed onboarding. This ensures that keyboard suppression
  // stops.
  SetShouldSuppressKeyboard(false);
}

bool FastCheckoutClientImpl::IsRunning() const {
  return is_running_;
}

std::unique_ptr<FastCheckoutExternalActionDelegate>
FastCheckoutClientImpl::CreateFastCheckoutExternalActionDelegate() {
  return std::make_unique<FastCheckoutExternalActionDelegate>();
}

std::unique_ptr<FastCheckoutController>
FastCheckoutClientImpl::CreateFastCheckoutController() {
  return std::make_unique<FastCheckoutControllerImpl>(&GetWebContents(), this);
}

std::unique_ptr<autofill_assistant::HeadlessScriptController>
FastCheckoutClientImpl::CreateHeadlessScriptController() {
  std::unique_ptr<autofill_assistant::AutofillAssistant> autofill_assistant =
      autofill_assistant::AutofillAssistantFactory::CreateForBrowserContext(
          GetWebContents().GetBrowserContext(),
          std::make_unique<autofill_assistant::CommonDependenciesChrome>(
              GetWebContents().GetBrowserContext()));
  return autofill_assistant->CreateHeadlessScriptController(
      &GetWebContents(), fast_checkout_external_action_delegate_.get());
}

void FastCheckoutClientImpl::OnHidden() {
  if (delegate_) {
    delegate_->OnFastCheckoutUIHidden();
  }
  SetShouldSuppressKeyboard(false);
}

void FastCheckoutClientImpl::OnOptionsSelected(
    std::unique_ptr<autofill::AutofillProfile> selected_profile,
    std::unique_ptr<autofill::CreditCard> selected_credit_card) {
  fast_checkout_external_action_delegate_->SetOptionsSelected(
      *selected_profile, *selected_credit_card);
  OnHidden();
}

void FastCheckoutClientImpl::OnDismiss() {
  OnHidden();
  Stop();
}

autofill::PersonalDataManager*
FastCheckoutClientImpl::GetPersonalDataManager() {
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext());
  return autofill::PersonalDataManagerFactory::GetForProfile(
      profile->GetOriginalProfile());
}

void FastCheckoutClientImpl::OnPersonalDataChanged() {
  if (!delegate_ || !delegate_->IsShowingFastCheckoutUI()) {
    return;
  }

  autofill::PersonalDataManager* pdm = GetPersonalDataManager();
  if (GetValidCreditCards(pdm).empty() ||
      GetValidAddressProfiles(pdm).empty()) {
    Stop();
  } else {
    ShowFastCheckoutUI();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FastCheckoutClientImpl);
