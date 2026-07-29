// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_PROVIDER_WIN_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_PROVIDER_WIN_H_

#include <map>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"

class GURL;
class ProofOfPossessionCookieInfo;

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace enterprise_auth {

class CloudApProviderWin : public PlatformAuthProvider {
 public:
  enum class SupportLevel {
    // Device does not support Cloud AP SSO.
    kUnsupported,
    // User has no accounts capable of SSO at the moment.
    kDisabled,
    // User has one or more accounts capable of SSO.
    kEnabled,
  };

  CloudApProviderWin();
  CloudApProviderWin(const CloudApProviderWin&) = delete;
  CloudApProviderWin& operator=(const CloudApProviderWin&) = delete;
  ~CloudApProviderWin() override;

  // enterprise_auth::PlatformAuthProvider:
  bool SupportsOriginFiltering() override;
  void FetchOrigins(FetchOriginsCallback on_fetch_complete) override;
  void GetData(const GURL& url,
               PlatformAuthProviderManager::GetDataCallback callback) override;

 private:
  friend class CloudApProviderWinTest;
  FRIEND_TEST_ALL_PREFIXES(CloudApProviderWinTest, Unsupported);
  FRIEND_TEST_ALL_PREFIXES(CloudApProviderWinTest, NotJoined);
  FRIEND_TEST_ALL_PREFIXES(CloudApProviderWinTest, Joined);
  FRIEND_TEST_ALL_PREFIXES(CloudApProviderWinTest, ParseCookieInfo);
  FRIEND_TEST_ALL_PREFIXES(CloudApProviderWinTest,
                           ParseCookieInfo_HeaderFeatureEnabled);

  // Runs the stored callbacks using the provided auth headers (legacy
  // non-queuing implementation).
  void OnGetDataCallback(net::HttpRequestHeaders auth_headers);

  // Maximum total number of pending data requests allowed across all URLs.
  static constexpr size_t kMaxQueueSize = 100;

  // Overrides support detection with `level` if it has a value, or resets the
  // override if not.
  static void SetSupportLevelForTesting(std::optional<SupportLevel> level);

  // Allows cookie info to be parsed for testing purposes.
  void ParseCookieInfoForTesting(const ProofOfPossessionCookieInfo* cookie_info,
                                 const DWORD cookie_info_count,
                                 net::HttpRequestHeaders& auth_headers);

  // List of callbacks to run when auth data is received (legacy non-queuing
  // implementation).
  using GetDataCallbackList =
      base::OnceCallbackList<void(net::HttpRequestHeaders)>;
  GetDataCallbackList on_get_data_callback_list_;

  // Subscriptions for auth data requests. Guarantees that the corresponding
  // callbacks are run on destruction (legacy non-queuing implementation).
  std::vector<base::CallbackListSubscription> get_data_subscriptions_;

  // Starts a background task to fetch auth data for `url`.
  void StartFetch(const GURL& url);

  // Handles completion of a background fetch for `url`.
  void OnFetchCompleted(const GURL& url, net::HttpRequestHeaders auth_headers);

  std::map<GURL, std::vector<PlatformAuthProviderManager::GetDataCallback>>
      request_queues_;

  // Total number of pending requests stored across all vectors in
  // `request_queues_`.
  size_t total_enqueued_requests_ = 0;

  base::WeakPtrFactory<CloudApProviderWin> weak_factory_{this};
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_PROVIDER_WIN_H_
