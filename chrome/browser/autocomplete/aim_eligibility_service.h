// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"

class PrefService;
class TemplateURLService;
class AimEligibilityServiceObserver;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

// Utility service to check if the user is eligible for certain features based
// on e.g. locale. Does not check feature states.
class AimEligibilityService : public KeyedService {
 public:
  AimEligibilityService(
      PrefService* pref_service,
      TemplateURLService* template_url_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AimEligibilityService() override;
  AimEligibilityService(const AimEligibilityService&) = delete;
  AimEligibilityService& operator=(const AimEligibilityService&) = delete;

  // Register observers to notify when eligibility may have changed. Eligibility
  // is re-checked periodically.
  void AddObserver(AimEligibilityServiceObserver* observer);
  void RemoveObserver(AimEligibilityServiceObserver* observer);

  // Verifies country and locale.
  static bool IsCountryAndLocale(const std::string& country,
                                 const std::string& locale);

  // Checks if user is eligible for AI mode e.g. in the omnibox or
  // chrome://settings/searchEngines.
  bool IsAimEligible() const;

  // Fetch eligibility from GWS server.
  void StartGwsRequest();
  void OnGwsResponse(std::unique_ptr<network::SimpleURLLoader> loader,
                     std::unique_ptr<std::string> response_string);

 private:
  // Notify `observers_` ineligibles may have changed.
  void NotifyObservers() const;

  base::ObserverList<AimEligibilityServiceObserver> observers_;
  const raw_ptr<PrefService> pref_service_;
  // Guaranteed not-null because this server `DependsOn` `TemplateURLService`.
  const raw_ptr<TemplateURLService> template_url_service_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Guaranteed not-null because this server `DependsOn` `TemplateURLService`.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // Update on each successfully GWS response.
  omnibox::AimEligibilityResponse most_recent_aim_eligibility_response;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_
