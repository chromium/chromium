// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

ChromeAimEligibilityService::ChromeAimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : AimEligibilityService(pref_service,
                            template_url_service,
                            url_loader_factory,
                            identity_manager) {}

ChromeAimEligibilityService::~ChromeAimEligibilityService() = default;

std::string ChromeAimEligibilityService::GetCountryCode() const {
  return base::ToLowerASCII(variations::GetCurrentCountryCode(
      g_browser_process->variations_service()));
}

std::string ChromeAimEligibilityService::GetLocale() const {
  return g_browser_process ? g_browser_process->GetFeatures()
                                 ->application_locale_storage()
                                 ->Get()
                           : "";
}
