// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_CHROME_AIM_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_CHROME_AIM_ELIGIBILITY_SERVICE_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"

// Concrete implementation of AimEligibilityService needed for fetching the
// country code and locale from the browser process.
class ChromeAimEligibilityService : public AimEligibilityService {
 public:
  ChromeAimEligibilityService(
      PrefService& pref_service,
      TemplateURLService& template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ChromeAimEligibilityService() override;

  // AimEligibilityService:
  std::string GetCountryCode() const override;
  std::string GetLocale() const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_CHROME_AIM_ELIGIBILITY_SERVICE_H_
