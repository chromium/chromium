// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service.h"

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/sync/sync_utils.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

constexpr char kUrlFilteringEndpointFlag[] = "url-filtering-endpoint";

std::optional<GURL> GetUrlOverride() {
  // Ignore this flag on Stable and Beta to avoid abuse.
  if (!g_browser_process || !g_browser_process->browser_policy_connector()
                                 ->IsCommandLineSwitchSupported()) {
    return std::nullopt;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kUrlFilteringEndpointFlag)) {
    GURL url =
        GURL(command_line->GetSwitchValueASCII(kUrlFilteringEndpointFlag));
    if (url.is_valid()) {
      return url;
    } else {
      LOG(ERROR) << "--" << kUrlFilteringEndpointFlag
                 << " is set to an invalid URL";
    }
  }

  return std::nullopt;
}

}  // namespace

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
    : RealTimeUrlLookupServiceBase(
          url_loader_factory,
          cache_manager,
          get_user_population_callback,
          referrer_chain_provider,
          /*pref_service=*/nullptr,
          /*webui_delegate=*/WebUIInfoSingleton::GetInstance()),
      profile_(profile),
      connectors_service_(connectors_service),
      token_fetcher_(std::move(token_fetcher)) {}

ChromeEnterpriseRealTimeUrlLookupService::
    ~ChromeEnterpriseRealTimeUrlLookupService() = default;

bool ChromeEnterpriseRealTimeUrlLookupService::CanPerformFullURLLookup() const {
  return RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
      profile_->GetPrefs(),
      connectors_service_->GetDMTokenForRealTimeUrlCheck().has_value(),
      profile_->IsOffTheRecord(), profile_->IsGuestSession());
}

bool ChromeEnterpriseRealTimeUrlLookupService::
    CanPerformFullURLLookupWithToken() const {
  DCHECK(CanPerformFullURLLookup());

  // Don't allow using the access token if the managed profile doesn't match the
  // managed device.
  if (policy::ManagementServiceFactory::GetForProfile(profile_)
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD_DOMAIN) &&
      !enterprise_util::IsProfileAffiliated(profile_)) {
    return false;
  }

  return safe_browsing::SyncUtils::IsPrimaryAccountSignedIn(
      IdentityManagerFactory::GetForProfile(profile_));
}

int ChromeEnterpriseRealTimeUrlLookupService::GetReferrerUserGestureLimit()
    const {
  return 2;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanSendPageLoadToken() const {
  // Page load token is disabled for enterprise users.
  return false;
}

bool ChromeEnterpriseRealTimeUrlLookupService::
    CanIncludeSubframeUrlInReferrerChain() const {
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
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    SessionID tab_id) {
  token_fetcher_->Start(base::BindOnce(
      &ChromeEnterpriseRealTimeUrlLookupService::OnGetAccessToken,
      weak_factory_.GetWeakPtr(), url, std::move(response_callback),
      std::move(callback_task_runner), base::TimeTicks::Now(), tab_id));
}

void ChromeEnterpriseRealTimeUrlLookupService::OnGetAccessToken(
    const GURL& url,
    RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    base::TimeTicks get_token_start_time,
    SessionID tab_id,
    const std::string& access_token) {
  MaybeSendRequest(url, access_token, std::move(response_callback),
                   std::move(callback_task_runner),
                   /* is_sampled_report */ false, tab_id);
}

std::optional<std::string>
ChromeEnterpriseRealTimeUrlLookupService::GetDMTokenString() const {
  DCHECK(connectors_service_);
  return connectors_service_->GetDMTokenForRealTimeUrlCheck();
}

GURL ChromeEnterpriseRealTimeUrlLookupService::GetRealTimeLookupUrl() const {
  return GetUrlOverride().value_or(
      GURL("https://enterprise-safebrowsing.googleapis.com/"
           "safebrowsing/clientreport/realtime"));
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

bool ChromeEnterpriseRealTimeUrlLookupService::CanCheckUrl(const GURL& url) {
  // Any URL can be checked in the enterprise case since URLs that might return
  // false when passed to `safe_browsing::CanGetReputationOfUrl` could still
  // trigger DLP rules. For example, this includes publicly routable IP
  // addresses.
  return true;
}

bool ChromeEnterpriseRealTimeUrlLookupService::ShouldIncludeCredentials()
    const {
  return false;
}

std::optional<base::Time> ChromeEnterpriseRealTimeUrlLookupService::
    GetMinAllowedTimestampForReferrerChains() const {
  // Enterprise URL lookup is enabled at startup and managed by the admin, so
  // all referrer URLs should be included in the referrer chain.
  return std::nullopt;
}

bool ChromeEnterpriseRealTimeUrlLookupService::CanSendRTSampleRequest() const {
  // Do not send sampled pings for enterprise users.
  return false;
}

std::string ChromeEnterpriseRealTimeUrlLookupService::GetUserEmail() const {
  return enterprise_connectors::GetProfileEmail(profile_);
}

std::string ChromeEnterpriseRealTimeUrlLookupService::GetBrowserDMTokenString()
    const {
  return connectors_service_->GetBrowserDmToken().value_or("");
}

std::string ChromeEnterpriseRealTimeUrlLookupService::GetProfileDMTokenString()
    const {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!connectors_service_->GetBrowserDmToken().has_value() ||
      enterprise_util::IsProfileAffiliated(profile_)) {
    return connectors_service_->GetProfileDmToken().value_or("");
  }
#endif
  return "";
}

std::unique_ptr<enterprise_connectors::ClientMetadata>
ChromeEnterpriseRealTimeUrlLookupService::GetClientMetadata() const {
  return connectors_service_->BuildClientMetadata(true);
}

}  // namespace safe_browsing
