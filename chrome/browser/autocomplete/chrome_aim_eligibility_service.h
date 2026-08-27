// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_CHROME_AIM_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_CHROME_AIM_ELIGIBILITY_SERVICE_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/aim_eligibility_service.h"

class PrefService;
class TemplateURLService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// Concrete implementation of AimEligibilityService needed for fetching the
// country code and locale from the browser process.
class ChromeAimEligibilityService : public AimEligibilityService {
 public:
  ChromeAimEligibilityService(
      PrefService& pref_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      Configuration configuration);
  ~ChromeAimEligibilityService() override;

  // AimEligibilityService:
  std::string GetLocaleImpl() const override;
  variations::VariationsService* GetVariationsService() const override;

 private:
  void OnLocaleChanged(const std::string& new_locale);

  base::CallbackListSubscription locale_change_subscription_;
  base::WeakPtrFactory<ChromeAimEligibilityService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_CHROME_AIM_ELIGIBILITY_SERVICE_H_
