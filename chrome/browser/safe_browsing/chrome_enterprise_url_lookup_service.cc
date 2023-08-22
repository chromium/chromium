// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
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
        std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
        enterprise_connectors::ConnectorsService* connectors_service,
        ReferrerChainProvider* referrer_chain_provider)
    : RealTimeUrlLookupServiceBase(url_loader_factory,
                                   cache_manager,
                                   get_user_population_callback,
                                   referrer_chain_provider,
                                   /* pref_service= */ nullptr),
      profile_(profile),
      connectors_service_(connectors_service),
      token_fetcher_(std::move(token_fetcher)) {}

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
  DCHECK(CanPerformFullURLLookup());

  // Don't allow using the access token if the managed profile doesn't match the
  // managed device.
  if (policy::ManagementServiceFactory::GetForProfile(profile_)
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) &&
      !chrome::enterprise_util::IsProfileAffiliated(profile_)) {
    return false;
  }

  if (safe_browsing::SyncUtils::IsPrimaryAccountSignedIn(
          IdentityManagerFactory::GetForProfile(profile_))) {
    return base::FeatureList::IsEnabled((kRealTimeUrlFilteringForEnterprise));
  }
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
  // Check database if safe browsing is enabled.
  return safe_browsing::IsSafeBrowsingEnabled(*profile_->GetPrefs());
}

bool ChromeEnterpriseRealTimeUrlLookupService::
    CanCheckSafeBrowsingHighConfidenceAllowlist() const {
  // Check allowlist if it can check database and allowlist bypass is
  // disabled.
  return CanCheckSafeBrowsingDb() && !CanPerformFullURLLookup();
}

void ChromeEnterpriseRealTimeUrlLookupService::GetAccessToken(
    const GURL& url,
    const GURL& last_committed_url,
    bool is_mainframe,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  DCHECK(base::FeatureList::IsEnabled((kRealTimeUrlFilteringForEnterprise)));
  token_fetcher_->Start(base::BindOnce(
      &ChromeEnterpriseRealTimeUrlLookupService::OnGetAccessToken,
      weak_factory_.GetWeakPtr(), url, last_committed_url, is_mainframe,
      std::move(request_callback), std::move(response_callback),
      std::move(callback_task_runner), base::TimeTicks::Now()));
}

void ChromeEnterpriseRealTimeUrlLookupService::OnGetAccessToken(
    const GURL& url,
    const GURL& last_committed_url,
    bool is_mainframe,
    RTLookupRequestCallback request_callback,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::TimeTicks get_token_start_time,
    const std::string& access_token) {
  SendRequest(url, last_committed_url, is_mainframe, access_token,
              std::move(request_callback), std::move(response_callback),
              std::move(callback_task_runner),
              /* is_sampled_report */ false);
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

void ChromeEnterpriseRealTimeUrlLookupService::Shutdown() {
  token_fetcher_.reset();
  RealTimeUrlLookupServiceBase::Shutdown();
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
