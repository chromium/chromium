// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_PROVIDER_WIN_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_PROVIDER_WIN_H_

#include <optional>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
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

  // Runs the stored callbacks using the provided auth headers.
  void OnGetDataCallback(net::HttpRequestHeaders auth_headers);

  // Overrides support detection with `level` if it has a value, or resets the
  // override if not.
  static void SetSupportLevelForTesting(std::optional<SupportLevel> level);

  // Allows cookie info to be parsed for testing purposes.
  void ParseCookieInfoForTesting(const ProofOfPossessionCookieInfo* cookie_info,
                                 const DWORD cookie_info_count,
                                 net::HttpRequestHeaders& auth_headers);

  // List of callbacks to run when auth data is received.
  using GetDataCallbackList =
      base::OnceCallbackList<void(net::HttpRequestHeaders)>;
  GetDataCallbackList on_get_data_callback_list_;

  // Subscription for auth data requests. Guarantees that the corresponding
  // callbacks are run on destruction.
  base::CallbackListSubscription get_data_subscription_;
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_PROVIDER_WIN_H_
