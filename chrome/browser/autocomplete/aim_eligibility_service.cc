// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/aim_eligibility_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_observer.h"
#include "chrome/browser/browser_process.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/service/variations_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "url/gurl.h"

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

static constexpr char kGwsRequestEndpoint[] =
    "http://www.google.com/async/folae?async=_fmt:pb";

const net::NetworkTrafficAnnotationTag kGwsRequestTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("aim_eligibility_fetch", R"(
      semantics {
        sender: "Chrome AI Mode Eligibility Service"
        description:
          "Retrieves the set of AI Mode features the client is eligible for "
          "from the server."
        trigger:
          "Requests are made on startup, when user's profile state changes, "
          "and periodically while Chrome is running."
        user_data {
          type: NONE
        }
        data:
          "No request body is sent; this is a GET request with no query params."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts { email: "chrome-desktop-search@google.com" }
        }
        last_reviewed: "2025-08-06"
      }
      policy {
        cookies_allowed: NO
        setting: "Coupled to Google default search."
        policy_exception_justification:
          "Not gated by policy. Setting AIModeSetting to '1' prevents the "
          "response from being used. But Google Chrome still makes the "
          "requests and saves the response to disk so that it's available when "
          "the policy is unset."
      })");

}  // namespace

AimEligibilityService::AimEligibilityService(
    PrefService* pref_service,
    TemplateURLService* template_url_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service),
      template_url_service_(template_url_service),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {
  // TODO(crbug.com/436898763): Call `StartGwsRequest()` to refresh the server
  //   response when service is constructed and when user state changes. E.g.
  //   user signs in/out, starts/stops syncing, switches profiles. Some of those
  //   actions may create a new service; if so, we don't need to listen to those
  //   events and start StartGwsRequest manually, because it'll be called in the
  //   constructor anyways. Switching profiles probably creates a new service.
}

AimEligibilityService::~AimEligibilityService() = default;

void AimEligibilityService::StartGwsRequest() {
  std::unique_ptr<network::ResourceRequest> request =
      std::make_unique<network::ResourceRequest>();
  request->url = GURL{kGwsRequestEndpoint};
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       kGwsRequestTrafficAnnotation);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&AimEligibilityService::OnGwsResponse,
                     base::Unretained(this), std::move(loader)));
}

void AimEligibilityService::OnGwsResponse(
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::unique_ptr<std::string> response_string) {
  // TODO(crbug.com/436900259): Add UMA metrics for whether the response
  //   returned 200, was parsed successfully, and which features were eligible.
  //   This will let us know how watered down UMA and finch are compared due to
  //   mismatched GWS eligibility criteria and estimate the actual population
  //   size.
  // TODO(crbug.com/436899694): Save the response to disk so we can enable AI
  //   features on startup without having to wait for a successful GWS response.
  if (!response_string)
    return;
  // Parse into a temporary variable 1st so that if parsing fails,
  // `most_recent_aim_eligibility_response` isn't cleared.
  omnibox::AimEligibilityResponse response_proto;
  if (!response_proto.ParseFromString(*response_string))
    return;
  most_recent_aim_eligibility_response = response_proto;
  NotifyObservers();
}

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
  // TODO(crbug.com/436901669): Conditionally check
  //   `most_recent_aim_eligibility_response.is_eligible()` and
  //   `IsCountryAndLocale()` depending on feature param state.
  return search::DefaultSearchProviderIsGoogle(template_url_service_) &&
         IsCountryAndLocale("us", "en-US") &&
         omnibox::IsAimAllowedByPolicy(pref_service_);
}

void AimEligibilityService::NotifyObservers() const {
  for (auto& observer : observers_)
    observer.OnAimEligibilityServiceChanged();
}
