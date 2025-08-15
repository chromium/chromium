// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;
class TemplateURLService;

// Utility service to check if the user is eligible for certain features based
// on e.g. locale. Does not check feature states.
class AimEligibilityService : public KeyedService {
 public:
  AimEligibilityService(PrefService* pref_service,
                        TemplateURLService* template_url_service);
  ~AimEligibilityService() override;
  AimEligibilityService(const AimEligibilityService&) = delete;
  AimEligibilityService& operator=(const AimEligibilityService&) = delete;

  // Verifies country and locale.
  static bool IsCountryAndLocale(const std::string& country,
                                 const std::string& locale);

  // Checks if user is eligible for AI mode e.g. in the omnibox or
  // chrome://settings/searchEngines.
  bool IsAimEligible() const;

 private:
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<TemplateURLService> template_url_service_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_
