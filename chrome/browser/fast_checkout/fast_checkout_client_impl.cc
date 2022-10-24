// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"
#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/fast_checkout/fast_checkout_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/fast_checkout_delegate.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"
#include "components/autofill_assistant/browser/public/external_action_util.h"
#include "components/autofill_assistant/browser/public/headless_onboarding_result.h"
#include "components/autofill_assistant/browser/public/public_script_parameters.h"
#include "components/autofill_assistant/browser/public/runtime_manager.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace {
constexpr char kIntentValue[] = "CHROME_FAST_CHECKOUT";
constexpr char kTrue[] = "true";
constexpr char kFalse[] = "false";
// TODO(crbug.com/1338521): Define and specify proper caller(s) and source(s).
constexpr char kCaller[] = "7";  // run was started from within Chromium
constexpr char kSource[] = "1";  // run was started organically
constexpr char kIsNoRoundTrip[] = "IS_NO_ROUND_TRIP";

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
    return !autofill_assistant::IsCompleteAddressProfile(profile,
                                                         pdm->app_locale());
  });
  return profiles;
}

// Create script parameters map for starting the script.
base::flat_map<std::string, std::string> CreateScriptParameters(
    bool run_consentless,
    GURL url) {
  base::flat_map<std::string, std::string> script_parameters = {
      {autofill_assistant::public_script_parameters::kIntentParameterName,
       kIntentValue},
      {autofill_assistant::public_script_parameters::
           kOriginalDeeplinkParameterName,
       url.spec()},
      {autofill_assistant::public_script_parameters::kEnabledParameterName,
       kTrue},
      {autofill_assistant::public_script_parameters::
           kStartImmediatelyParameterName,
       kTrue},
      {autofill_assistant::public_script_parameters::kCallerParameterName,
       kCaller},
      {autofill_assistant::public_script_parameters::kSourceParameterName,
       kSource},
      {kIsNoRoundTrip, run_consentless ? kTrue : kFalse}};

  // TODO(b/251365675): Remove once all endpoints support RPC signing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAutofillAssistantUrl)) {
    script_parameters.insert({autofill_assistant::public_script_parameters::
                                  kDisableRpcSigningParameterName,
                              kTrue});
  }

  return script_parameters;
}

}  // namespace

FastCheckoutClientImpl::FastCheckoutClientImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<FastCheckoutClientImpl>(*web_contents),
      fast_checkout_prefs_(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs()) {}

FastCheckoutClientImpl::~FastCheckoutClientImpl() {
  if (is_running_) {
    base::UmaHistogramEnumeration(kUmaKeyFastCheckoutRunOutcome,
                                  FastCheckoutRunOutcome::kIncompleteRun);
  }
}

bool FastCheckoutClientImpl::Start(
    base::WeakPtr<autofill::FastCheckoutDelegate> delegate,
    const GURL& url,
    bool script_supports_consentless_execution) {
  if (!ShouldRun(script_supports_consentless_execution)) {
    LOG_AF(GetAutofillLogManager()) << autofill::LoggingScope::kFastCheckout
                                    << autofill::LogMessage::kFastCheckout
                                    << "not triggered because "
                                       "`ShouldRun()` returned `false`.";
    return false;
  }

  bool run_consentless =
      features::kFastCheckoutConsentlessExecutionParam.Get() &&
      script_supports_consentless_execution;
  is_running_ = true;
  url_ = url;
  delegate_ = std::move(delegate);
  personal_data_manager_observation_.Observe(GetPersonalDataManager());

  fast_checkout_external_action_delegate_ =
      CreateFastCheckoutExternalActionDelegate();
  external_script_controller_ = CreateHeadlessScriptController();

  SetShouldSuppressKeyboard(true);

  external_script_controller_->StartScript(
      CreateScriptParameters(run_consentless, url_),
      base::BindOnce(&FastCheckoutClientImpl::OnRunComplete,
                     base::Unretained(this)),
      /*use_autofill_assistant_onboarding=*/!run_consentless,
      base::BindOnce(&FastCheckoutClientImpl::OnOnboardingCompletedSuccessfully,
                     base::Unretained(this)),
      /*suppress_browsing_features=*/false);

  return true;
}

bool FastCheckoutClientImpl::ShouldRun(
    bool script_supports_consentless_execution) {
  if (!base::FeatureList::IsEnabled(features::kFastCheckout)) {
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "not triggered because FastCheckout flag is disabled.";
    return false;
  }

  bool client_supports_consentless_execution =
      features::kFastCheckoutConsentlessExecutionParam.Get();

  // The run requires consent (`script_supports_consentless_execution == false`)
  // but the client is consentless.
  if (!script_supports_consentless_execution &&
      client_supports_consentless_execution) {
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "not triggered because the script requires consent but the client "
           "is consent-less.";
    return false;
  }

  if (is_running_) {
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "not triggered because Fast Checkout is already running.";
    return false;
  }

  // Client requires consent and has declined onboarding previously.
  if (fast_checkout_prefs_.IsOnboardingDeclined() &&
      !client_supports_consentless_execution) {
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "not triggered because the client requires consent and has declined "
           "onboarding previously.";
    return false;
  }

  autofill::PersonalDataManager* pdm = GetPersonalDataManager();
  DCHECK(pdm);
  // Trigger only if there is at least 1 valid Autofill profile on file.
  if (GetValidAddressProfiles(pdm).empty()) {
    base::UmaHistogramEnumeration(
        autofill::kUmaKeyFastCheckoutTriggerOutcome,
        autofill::FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile);
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "not triggered because the client does not have at least one valid "
           "Autofill profile stored.";
    return false;
  }
  // Trigger only if there is at least 1 complete valid credit card on file.
  if (GetValidCreditCards(pdm).empty()) {
    base::UmaHistogramEnumeration(
        autofill::kUmaKeyFastCheckoutTriggerOutcome,
        autofill::FastCheckoutTriggerOutcome::kFailureNoValidCreditCard);
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "not triggered because the client does not have at least one "
           "valid Autofill credit card stored.";
    return false;
  }

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

  GetRuntimeManager()->SetUIState(
      autofill_assistant::UIState::kShownWithoutBrowsingFeatureSuppression);
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
  if (result.onboarding_result ==
      autofill_assistant::HeadlessOnboardingResult::kRejected) {
    fast_checkout_prefs_.DeclineOnboarding();
    base::UmaHistogramEnumeration(kUmaKeyFastCheckoutRunOutcome,
                                  FastCheckoutRunOutcome::kOnboardingDeclined);
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "run failed because onboarding was rejected.";
  } else if (result.success) {
    base::UmaHistogramEnumeration(kUmaKeyFastCheckoutRunOutcome,
                                  FastCheckoutRunOutcome::kSuccess);
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout << "run was successful.";
  } else {
    base::UmaHistogramEnumeration(kUmaKeyFastCheckoutRunOutcome,
                                  FastCheckoutRunOutcome::kFail);
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout << "run failed.";
  }

  OnHidden();
  Stop();
}

void FastCheckoutClientImpl::Stop() {
  external_script_controller_.reset();
  fast_checkout_controller_.reset();
  is_running_ = false;
  personal_data_manager_observation_.Reset();
  GetRuntimeManager()->SetUIState(autofill_assistant::UIState::kNotShown);

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

autofill_assistant::RuntimeManager*
FastCheckoutClientImpl::GetRuntimeManager() {
  return autofill_assistant::RuntimeManager::GetOrCreateForWebContents(
      &GetWebContents());
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

autofill::LogManager* FastCheckoutClientImpl::GetAutofillLogManager() {
  if (!delegate_)
    return nullptr;

  autofill::ContentAutofillDriver* driver =
      static_cast<autofill::ContentAutofillDriver*>(delegate_->GetDriver());

  if (!driver)
    return nullptr;

  return driver->autofill_manager()->client()->GetLogManager();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FastCheckoutClientImpl);
