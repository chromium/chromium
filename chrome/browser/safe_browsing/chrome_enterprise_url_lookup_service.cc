// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"

#include "base/callback.h"
#include "base/task/post_task.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/thread_utils.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/realtime/policy_engine.h"
#include "components/safe_browsing/core/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace safe_browsing {

ChromeEnterpriseRealTimeUrlLookupService::
    ChromeEnterpriseRealTimeUrlLookupService(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        VerdictCacheManager* cache_manager,
        Profile* profile,
        base::RepeatingCallback<ChromeUserPopulation()>
            get_user_population_callback,
        enterprise_connectors::ConnectorsService* connectors_service,
        ReferrerChainProvider* referrer_chain_provider)
    : RealTimeUrlLookupServiceBase(url_loader_factory,
                                   cache_manager,
                                   get_user_population_callback,
                                   referrer_chain_provider),
      profile_(profile),
      connectors_service_(connectors_service) {}

ChromeEnterpriseRealTimeUrlLookupService::
    ~ChromeEnterpriseRealTimeUrlLookupService() = default;

bool ChromeEnterpriseRealTimeUrlLookupService::CanPerformFullURLLookup() const {
  return RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
      profile_->GetPrefs(),
      connectors_service_->GetDMTokenForRealTimeUrlCheck().has_value(),
      profile_->IsOffTheRecord());
}

bool ChromeEnterpriseRealTimeUrlLookupService::
    CanPerformFullURLLookupWithToken() const {
  // URL lookup with token is disabled for enterprise users.
  return false;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanAttachReferrerChain() const {
  // Referrer chain is currently not supported for enterprise users.
  return false;
}

int ChromeEnterpriseRealTimeUrlLookupService::GetReferrerUserGestureLimit()
    const {
  NOTREACHED()
      << "Referrer chain is currently not supported for enterprise users.";
  return 0;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanCheckSubresourceURL() const {
  return false;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanCheckSafeBrowsingDb() const {
  return safe_browsing::IsSafeBrowsingEnabled(*profile_->GetPrefs());
}

void ChromeEnterpriseRealTimeUrlLookupService::GetAccessToken(
    const GURL& url,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback) {
  NOTREACHED() << "URL lookup with token is disabled for enterprise users.";
}

base::Optional<std::string>
ChromeEnterpriseRealTimeUrlLookupService::GetDMTokenString() const {
  DCHECK(connectors_service_);
  return connectors_service_->GetDMTokenForRealTimeUrlCheck();
}

GURL ChromeEnterpriseRealTimeUrlLookupService::GetRealTimeLookupUrl() const {
  bool is_ga_endpoint_enabled =
      base::FeatureList::IsEnabled(kRealTimeUrlLookupEnterpriseGaEndpoint);
  std::string endpoint = is_ga_endpoint_enabled
                             ? "https://enterprise-safebrowsing.googleapis.com/"
                               "safebrowsing/clientreport/realtime"
                             : "https://safebrowsing.google.com/safebrowsing/"
                               "clientreport/realtime";
  return GURL(endpoint);
}

net::NetworkTrafficAnnotationTag
ChromeEnterpriseRealTimeUrlLookupService::GetTrafficAnnotationTag() const {
  // Safe Browsing Zwieback cookies are not sent for enterprise users, because
  // DM tokens are sufficient for identification purposes.
  return net::DefineNetworkTrafficAnnotation(
      "enterprise_safe_browsing_realtime_url_lookup",
      R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "This is an enterprise-only feature. "
            "When Safe Browsing can't detect that a URL is safe based on its "
            "local database, it sends the top-level URL to Google to verify it "
            "before showing a warning to the user."
          trigger:
            "When the enterprise policy EnterpriseRealTimeUrlCheckMode is set "
            "and a main frame URL fails to match the local hash-prefix "
            "database of known safe URLs and a valid result from a prior "
            "lookup is not already cached, this will be sent."
          data:
            "The main frame URL that did not match the local safelist and "
            "the DM token of the device."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This is disabled by default and can only be enabled by policy "
            "through the Google Admin console."
          chrome_policy {
            EnterpriseRealTimeUrlCheckMode {
              EnterpriseRealTimeUrlCheckMode: 0
            }
          }
        })");
}

std::string ChromeEnterpriseRealTimeUrlLookupService::GetMetricSuffix() const {
  return ".Enterprise";
}

bool ChromeEnterpriseRealTimeUrlLookupService::ShouldIncludeCredentials()
    const {
  return !base::FeatureList::IsEnabled(kSafeBrowsingRemoveCookies);
}

}  // namespace safe_browsing
