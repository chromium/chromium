// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/promo_handler.h"

#include "base/feature_list.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/companion/core/signin_delegate.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"

namespace companion {

PromoHandler::PromoHandler(PrefService* pref_service,
                           SigninDelegate* signin_delegate)
    : pref_service_(pref_service), signin_delegate_(signin_delegate) {}

PromoHandler::~PromoHandler() = default;

// static
void PromoHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kMsbbPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kSigninPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kExpsPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kExpsPromoShownCountPref, 0);
  registry->RegisterIntegerPref(kPcoPromoShownCountPref, 0);
  registry->RegisterIntegerPref(kPcoPromoDeclinedCountPref, 0);
  // TODO(shaktisahu): Move the pref registration to a better location.
  registry->RegisterBooleanPref(kExpsOptInStatusGrantedPref, false);
  registry->RegisterBooleanPref(kHasNavigatedToExpsSuccessPage, false);
}

void PromoHandler::OnPromoAction(PromoType promo_type,
                                 PromoAction promo_action) {
  switch (promo_type) {
    case PromoType::kSignin:
      OnSigninPromo(promo_action);
      return;
    case PromoType::kMsbb:
      OnMsbbPromo(promo_action);
      return;
    case PromoType::kExps:
      OnExpsPromo(promo_action);
      return;
    case PromoType::kPco:
      OnPcoPromo(promo_action);
      return;
    default:
      return;
  }
}

void PromoHandler::OnSigninPromo(PromoAction promo_action) {
  switch (promo_action) {
    case PromoAction::kRejected:
      IncrementPref(kSigninPromoDeclinedCountPref);
      return;
    case PromoAction::kAccepted:
      signin_delegate_->StartSigninFlow();
      return;
    default:
      return;
  }
}

void PromoHandler::OnMsbbPromo(PromoAction promo_action) {
  switch (promo_action) {
    case PromoAction::kRejected:
      IncrementPref(kMsbbPromoDeclinedCountPref);
      return;
    case PromoAction::kAccepted:
      // Turn on MSBB.
      signin_delegate_->EnableMsbb(true);
      return;
    default:
      return;
  }
}

void PromoHandler::OnExpsPromo(PromoAction promo_action) {
  switch (promo_action) {
    case PromoAction::kShown:
      IncrementPref(kExpsPromoShownCountPref);
      return;
    case PromoAction::kRejected:
      IncrementPref(kExpsPromoDeclinedCountPref);
      return;
    default:
      return;
  }
}

void PromoHandler::OnPcoPromo(PromoAction promo_action) {
  switch (promo_action) {
    case PromoAction::kShown:
      IncrementPref(kPcoPromoShownCountPref);
      return;
    case PromoAction::kRejected:
      IncrementPref(kPcoPromoDeclinedCountPref);
      return;
    case PromoAction::kAccepted:
      // The promo shouldn't be shown unless the user has this feature enabled.
      // But since this relies on google3 code, it's safer to use guard instead
      // of `CHECK` and crash.
      if (base::FeatureList::IsEnabled(features::kCompanionEnablePageContent)) {
        pref_service_->SetBoolean(
            unified_consent::prefs::kPageContentCollectionEnabled, true);
      }
      return;
  }
}

void PromoHandler::IncrementPref(const std::string& pref_name) {
  int current_val = pref_service_->GetInteger(pref_name);
  pref_service_->SetInteger(pref_name, current_val + 1);
}

}  // namespace companion
