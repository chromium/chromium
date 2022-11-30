// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_TOKEN_CACHE_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_TOKEN_CACHE_H_

#include <map>
#include <set>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace extensions {

class IdentityTokenCacheValue {
 public:
  IdentityTokenCacheValue();
  IdentityTokenCacheValue(const IdentityTokenCacheValue& other);
  IdentityTokenCacheValue& operator=(const IdentityTokenCacheValue& other);
  ~IdentityTokenCacheValue();

  static IdentityTokenCacheValue CreateRemoteConsent(
      const RemoteConsentResolutionData& resolution_data);
  static IdentityTokenCacheValue CreateRemoteConsentApproved(
      const std::string& consent_result);
  static IdentityTokenCacheValue CreateToken(
      const std::string& token,
      const std::set<std::string>& granted_scopes,
      base::TimeDelta time_to_live);

  // Order of these entries is used to determine whether or not new
  // entries supersede older ones in SetCachedToken.
  enum CacheValueStatus {
    CACHE_STATUS_NOTFOUND,
    CACHE_STATUS_REMOTE_CONSENT,
    CACHE_STATUS_REMOTE_CONSENT_APPROVED,
    CACHE_STATUS_TOKEN
  };

  CacheValueStatus status() const;
  const base::Time& expiration_time() const;

  // These getters should be used only if `status()` returns a value
  // corresponding to the type. Otherwise, the application will crash.
  // CACHE_STATUS_REMOTE_CONSENT:
  const RemoteConsentResolutionData& resolution_data() const;
  // CACHE_STATUS_REMOTE_CONSENT_APPROVED:
  const std::string& consent_result() const;
  // CACHE_STATUS_TOKEN:
  const std::string& token() const;
  const std::set<std::string>& granted_scopes() const;

 private:
  struct TokenValue {
    TokenValue(const std::string& token,
               const std::set<std::string>& granted_scopes);
    TokenValue(const TokenValue& other);
    TokenValue& operator=(const TokenValue& other);
    ~TokenValue();

    std::string token;
    std::set<std::string> granted_scopes;
  };

  CacheValueStatus GetStatusInternal() const;

  bool is_expired() const;

  base::Time expiration_time_;

  absl::variant<absl::monostate,
                RemoteConsentResolutionData,
                std::string,
                TokenValue>
      value_;
};

// In-memory cache of OAuth2 access tokens that are requested by extensions
// through the `getAuthToken` API. Also caches intermediate short-lived values
// used at different stages of the `getAuthToken` flow before a token is
// obtained. The cache automatically handles token expiration. Extensions can
// manually remove tokens from the cache using `removeCachedAuthToken` API.
//
// chrome://identity-internals provides a view of cache's content for debugging.
class IdentityTokenCache {
 public:
  IdentityTokenCache();
  ~IdentityTokenCache();
  IdentityTokenCache(const IdentityTokenCache& other) = delete;
  IdentityTokenCache& operator=(const IdentityTokenCache& other) = delete;

  // Used to order the cached scopes based on their sizes. This allows subset
  // matching to prioritize returning tokens with the smallest superset scope.
  // So, if there is an exact match for the requested scopes, the corresponding
  // cached token will be found first and returned.
  struct ScopesSizeCompare {
    bool operator()(const IdentityTokenCacheValue& lhs,
                    const IdentityTokenCacheValue& rhs) const;
  };

  struct AccessTokensKey {
    explicit AccessTokensKey(const ExtensionTokenKey& key);
    AccessTokensKey(const std::string& extension_id,
                    const CoreAccountId& account_id);
    bool operator<(const AccessTokensKey& rhs) const;
    std::string extension_id;
    CoreAccountId account_id;
  };
  using AccessTokensValue =
      std::set<IdentityTokenCacheValue, ScopesSizeCompare>;
  using AccessTokensCache = std::map<AccessTokensKey, AccessTokensValue>;

  void SetToken(const ExtensionTokenKey& key,
                const IdentityTokenCacheValue& token_data);
  void EraseAccessToken(const std::string& extension_id,
                        const std::string& token);
  void EraseAllTokensForExtension(const std::string& extension_id);
  void EraseAllTokens();
  const IdentityTokenCacheValue& GetToken(const ExtensionTokenKey& key);

  const AccessTokensCache& access_tokens_cache();

 private:
  void EraseStaleTokens();

  AccessTokensCache access_tokens_cache_;
  std::map<ExtensionTokenKey, IdentityTokenCacheValue>
      intermediate_value_cache_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_TOKEN_CACHE_H_
