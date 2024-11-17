// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class Profile;

namespace safe_browsing {

class ReferrerChainProvider;

// This class implements the real time lookup feature for a given user/profile.
// It is separated from the base class for logic that is related to enterprise
// users.(See: go/chrome-protego-enterprise-dd)
class ChromeEnterpriseRealTimeUrlLookupService
    : public RealTimeUrlLookupServiceBase {
 public:
  ChromeEnterpriseRealTimeUrlLookupService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      VerdictCacheManager* cache_manager,
      Profile* profile,
      base::RepeatingCallback<ChromeUserPopulation()>
          get_user_population_callback,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      enterprise_connectors::ConnectorsService* connectors_service,
      ReferrerChainProvider* referrer_chain_provider);

  ChromeEnterpriseRealTimeUrlLookupService(
      const ChromeEnterpriseRealTimeUrlLookupService&) = delete;
  ChromeEnterpriseRealTimeUrlLookupService& operator=(
      const ChromeEnterpriseRealTimeUrlLookupService&) = delete;

  ~ChromeEnterpriseRealTimeUrlLookupService() override;

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override;
  bool CanIncludeSubframeUrlInReferrerChain() const override;
  bool CanCheckSafeBrowsingDb() const override;
  bool CanCheckSafeBrowsingHighConfidenceAllowlist() const override;
  bool CanSendRTSampleRequest() const override;
  std::string GetUserEmail() const override;
  std::string GetBrowserDMTokenString() const override;
  std::string GetProfileDMTokenString() const override;
  std::unique_ptr<enterprise_connectors::ClientMetadata> GetClientMetadata()
      const override;
  std::string GetMetricSuffix() const override;
  void Shutdown() override;
  bool CanCheckUrl(const GURL& url) override;

 private:
  // RealTimeUrlLookupServiceBase:
  GURL GetRealTimeLookupUrl() const override;
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override;
  bool CanPerformFullURLLookupWithToken() const override;
  int GetReferrerUserGestureLimit() const override;
  bool CanSendPageLoadToken() const override;
  void GetAccessToken(
      const GURL& url,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID tab_id) override;

  // Called when the access token is obtained from |token_fetcher_|.
  void OnGetAccessToken(
      const GURL& url,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      base::TimeTicks get_token_start_time,
      SessionID tab_id,
      const std::string& access_token);

  std::optional<std::string> GetDMTokenString() const override;
  bool ShouldIncludeCredentials() const override;
  std::optional<base::Time> GetMinAllowedTimestampForReferrerChains()
      const override;

  // Unowned object used for checking profile based settings.
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // Unowned pointer to ConnectorsService, used to get a DM token.
  raw_ptr<enterprise_connectors::ConnectorsService, DanglingUntriaged>
      connectors_service_;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  friend class ChromeEnterpriseRealTimeUrlLookupServiceTest;

  base::WeakPtrFactory<ChromeEnterpriseRealTimeUrlLookupService> weak_factory_{
      this};

};  // class ChromeEnterpriseRealTimeUrlLookupService

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_ENTERPRISE_URL_LOOKUP_SERVICE_H_
