// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/fast_checkout_delegate.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace {
bool IsCompleteAddressProfile(const autofill::AutofillProfile* profile,
                              const std::string& app_locale) {
  std::string country_code =
      base::UTF16ToASCII(profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  if (country_code.empty()) {
    return false;
  }

  autofill::AutofillCountry country(country_code, app_locale);
  return !profile->GetInfo(autofill::NAME_FULL, app_locale).empty() &&
         !profile->GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS).empty() &&
         (!country.requires_zip() ||
          profile->HasRawInfo(autofill::ADDRESS_HOME_ZIP)) &&
         !profile->GetRawInfo(autofill::EMAIL_ADDRESS).empty() &&
         !profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER).empty();
}

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
    return !IsCompleteAddressProfile(profile, pdm->app_locale());
  });
  return profiles;
}

}  // namespace

FastCheckoutClientImpl::FastCheckoutClientImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<FastCheckoutClientImpl>(*web_contents) {}

FastCheckoutClientImpl::~FastCheckoutClientImpl() {
  if (is_running_) {
    base::UmaHistogramEnumeration(kUmaKeyFastCheckoutRunOutcome,
                                  FastCheckoutRunOutcome::kIncompleteRun);
  }
}

bool FastCheckoutClientImpl::Start(
    base::WeakPtr<autofill::FastCheckoutDelegate> delegate,
    const GURL& url) {
  if (!ShouldRun()) {
    LOG_AF(GetAutofillLogManager()) << autofill::LoggingScope::kFastCheckout
                                    << autofill::LogMessage::kFastCheckout
                                    << "not triggered because "
                                       "`ShouldRun()` returned `false`.";
    return false;
  }

  is_running_ = true;
  url_ = url;
  delegate_ = std::move(delegate);
  personal_data_manager_observation_.Observe(GetPersonalDataManager());

  SetShouldSuppressKeyboard(true);

  fast_checkout_controller_ = CreateFastCheckoutController();
  ShowFastCheckoutUI();

  return true;
}

bool FastCheckoutClientImpl::ShouldRun() {
  if (!base::FeatureList::IsEnabled(features::kFastCheckout)) {
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "not triggered because FastCheckout flag is disabled.";
    return false;
  }

  if (is_running_) {
    LOG_AF(GetAutofillLogManager())
        << autofill::LoggingScope::kFastCheckout
        << autofill::LogMessage::kFastCheckout
        << "not triggered because Fast Checkout is already running.";
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

void FastCheckoutClientImpl::OnRunComplete() {
  // TODO(crbug.com/1334642): Handle result (e.g. report metrics).
  OnHidden();
  Stop();
}

void FastCheckoutClientImpl::Stop() {
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

std::unique_ptr<FastCheckoutController>
FastCheckoutClientImpl::CreateFastCheckoutController() {
  return std::make_unique<FastCheckoutControllerImpl>(&GetWebContents(), this);
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
  // TODO(crbug.com/1334642): Signal that FC options have been selected.
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
