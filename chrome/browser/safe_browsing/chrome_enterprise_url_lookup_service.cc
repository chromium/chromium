// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"

#include "base/callback.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
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

int ChromeEnterpriseRealTimeUrlLookupService::GetReferrerUserGestureLimit()
    const {
  return 2;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanSendPageLoadToken() const {
  // Page load token is disabled for enterprise users.
  return false;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanCheckSubresourceURL() const {
  return false;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanCheckSafeBrowsingDb() const {
  return safe_browsing::IsSafeBrowsingEnabled(*profile_->GetPrefs());
}

void ChromeEnterpriseRealTimeUrlLookupService::GetAccessToken(
    const GURL& url,
    const GURL& last_committed_url,
    bool is_mainframe,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  NOTREACHED() << "URL lookup with token is disabled for enterprise users.";
}

absl::optional<std::string>
ChromeEnterpriseRealTimeUrlLookupService::GetDMTokenString() const {
  DCHECK(connectors_service_);
  return connectors_service_->GetDMTokenForRealTimeUrlCheck();
}

GURL ChromeEnterpriseRealTimeUrlLookupService::GetRealTimeLookupUrl() const {
  std::string endpoint =
      "https://enterprise-safebrowsing.googleapis.com/"
      "safebrowsing/clientreport/realtime";
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
  return false;
}

double ChromeEnterpriseRealTimeUrlLookupService::
    GetMinAllowedTimestampForReferrerChains() const {
  // Enterprise URL lookup is enabled at startup and managed by the admin, so
  // all referrer URLs should be included in the referrer chain.
  return 0;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanSendRTSampleRequest() const {
  // Do not send sampled pings for enterprise users.
  return false;
}

}  // namespace safe_browsing
