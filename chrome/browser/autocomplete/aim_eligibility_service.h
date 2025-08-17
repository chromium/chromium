// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"

class AimEligibilityServiceObserver;
class PrefRegistrySimple;
class PrefService;
class TemplateURLService;

// If disabled, AIM is completely turned off (kill switch).
BASE_DECLARE_FEATURE(kAimEnabled);

// If enabled, uses the server response for AIM eligibility.
BASE_DECLARE_FEATURE(kAimServerEligibilityEnabled);

// If enabled, uses the server response for AIM eligibility for English locales.
BASE_DECLARE_FEATURE(kAimServerEligibilityEnabledEn);

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// Utility service to check if the user is eligible for certain AI mode features
// based on e.g. locale. Does not check feature states.
class AimEligibilityService : public KeyedService {
 public:
  // See comment for `WriteToPref()`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  AimEligibilityService(
      PrefService& pref_service,
      TemplateURLService& template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AimEligibilityService() override;
  AimEligibilityService(const AimEligibilityService&) = delete;
  AimEligibilityService& operator=(const AimEligibilityService&) = delete;

  // Checks if the application country matches the given country.
  static bool IsCountry(const std::string& country);
  // Checks if the application language matches the given language.
  static bool IsLanguage(const std::string& language);

  // Register observers to notify when eligibility may have changed. Eligibility
  // is re-checked periodically.
  void AddObserver(AimEligibilityServiceObserver* observer);
  void RemoveObserver(AimEligibilityServiceObserver* observer);

  // Checks if server eligibility checking is enabled.
  // Virtual for testing purposes.
  virtual bool IsServerEligibilityEnabled() const;

  // Checks if user is locally eligible for AI mode (excludes server checks).
  // Virtual for testing purposes.
  virtual bool IsAimLocallyEligible() const;

  // Checks if user is eligible for AI mode (includes server checks).
  // Virtual for testing purposes.
  virtual bool IsAimEligible() const;

 private:
  // Notify `observers_` ineligibles may have changed.
  void NotifyObservers() const;

  // Parses `response_string` into `most_recent_response_`. Does not modify
  // `most_recent_response_` if parsing fails.
  bool ParseResponseString(const std::string& response_string);

  // Write and read `most_recent_response_` to pref so it can be used when GWS
  // server is unavailable.
  void WriteToPref(const std::string& response_string) const;
  void ReadFromPref();

  // Fetch eligibility from GWS server.
  void StartGwsRequest();
  void OnGwsResponse(std::unique_ptr<network::SimpleURLLoader> loader,
                     std::unique_ptr<std::string> response_string);

  static constexpr char kResponsePrefName[] =
      "aim_eligibility_service.aim_eligibility_response";

  base::ObserverList<AimEligibilityServiceObserver> observers_;
  const raw_ref<PrefService> pref_service_;
  // Guaranteed not-null because this server `DependsOn` `TemplateURLService`.
  const raw_ref<TemplateURLService> template_url_service_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Update on each successfully GWS response.
  omnibox::AimEligibilityResponse most_recent_response_;

  // For binding the `OnGwsResponse()` callback.
  base::WeakPtrFactory<AimEligibilityService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AIM_ELIGIBILITY_SERVICE_H_
