// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/aim_eligibility_service.h"

#include <string>

#include "chrome/browser/autocomplete/aim_eligibility_service_observer.h"
#include "chrome/browser/browser_process.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"

namespace {

// Returns the country code from the variations service.
std::string GetCountryCode() {
  std::string country_code;
  // The variations service may be nullptr in unit tests.
  variations::VariationsService* variations_service =
      g_browser_process ? g_browser_process->variations_service() : nullptr;
  if (variations_service) {
    country_code = variations_service->GetStoredPermanentCountry();
    if (country_code.empty()) {
      country_code = variations_service->GetLatestCountry();
    }
  }
  return country_code;
}

}  // namespace

AimEligibilityService::AimEligibilityService(
    PrefService* pref_service,
    TemplateURLService* template_url_service)
    : pref_service_(pref_service),
      template_url_service_(template_url_service) {}

AimEligibilityService::~AimEligibilityService() = default;

void AimEligibilityService::AddObserver(
    AimEligibilityServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void AimEligibilityService::RemoveObserver(
    AimEligibilityServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AimEligibilityService::IsCountryAndLocale(const std::string& country,
                                               const std::string& locale) {
  return g_browser_process &&
         g_browser_process->GetApplicationLocale() == locale &&
         GetCountryCode() == country;
}

bool AimEligibilityService::IsAimEligible() const {
  return search::DefaultSearchProviderIsGoogle(template_url_service_) &&
         IsCountryAndLocale("us", "en-US") &&
         omnibox::IsAimAllowedByPolicy(pref_service_);
}

void AimEligibilityService::NotifyObservers() const {
  for (auto& observer : observers_)
    observer.OnAimEligibilityServiceChanged();
}
