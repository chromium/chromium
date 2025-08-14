// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

ChromeAimEligibilityService::ChromeAimEligibilityService(
    PrefService& pref_service,
    TemplateURLService& template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : AimEligibilityService(pref_service,
                            template_url_service,
                            url_loader_factory) {}

ChromeAimEligibilityService::~ChromeAimEligibilityService() = default;

std::string ChromeAimEligibilityService::GetCountryCode() const {
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

std::string ChromeAimEligibilityService::GetLocale() const {
  return g_browser_process ? g_browser_process->GetFeatures()
                                 ->application_locale_storage()
                                 ->Get()
                           : "";
}
