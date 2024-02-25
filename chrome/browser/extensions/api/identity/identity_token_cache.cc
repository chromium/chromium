// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_token_cache.h"

#include <map>
#include <set>

#include "base/ranges/algorithm.h"
#include "chrome/browser/extensions/api/identity/identity_constants.h"

namespace extensions {

IdentityTokenCacheValue::IdentityTokenCacheValue() = default;
IdentityTokenCacheValue::IdentityTokenCacheValue(
    const IdentityTokenCacheValue& other) = default;
IdentityTokenCacheValue& IdentityTokenCacheValue::operator=(
    const IdentityTokenCacheValue& other) = default;
IdentityTokenCacheValue::~IdentityTokenCacheValue() = default;

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateRemoteConsent(
    const RemoteConsentResolutionData& resolution_data) {
  IdentityTokenCacheValue cache_value;
  cache_value.value_ = resolution_data;
  cache_value.expiration_time_ =
      base::Time::Now() +
      base::Seconds(identity_constants::kCachedRemoteConsentTTLSeconds);
  return cache_value;
}

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateRemoteConsentApproved(
    const std::string& consent_result) {
  IdentityTokenCacheValue cache_value;
  cache_value.value_ = consent_result;
  cache_value.expiration_time_ =
      base::Time::Now() +
      base::Seconds(identity_constants::kCachedRemoteConsentTTLSeconds);
  return cache_value;
}

// static
IdentityTokenCacheValue IdentityTokenCacheValue::CreateToken(
    const std::string& token,
    const std::set<std::string>& granted_scopes,
    base::TimeDelta time_to_live) {
  DCHECK(!granted_scopes.empty());

  IdentityTokenCacheValue cache_value;
  cache_value.value_ = TokenValue(token, granted_scopes);

  // Remove 20 minutes from the ttl so cached tokens will have some time
  // to live any time they are returned.
  time_to_live -= base::Minutes(20);

  base::TimeDelta zero_delta;
  if (time_to_live < zero_delta)
    time_to_live = zero_delta;

  cache_value.expiration_time_ = base::Time::Now() + time_to_live;
  return cache_value;
}

IdentityTokenCacheValue::CacheValueStatus IdentityTokenCacheValue::status()
    const {
  if (is_expired())
    return IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND;

  return GetStatusInternal();
}

IdentityTokenCacheValue::CacheValueStatus
IdentityTokenCacheValue::GetStatusInternal() const {
  if (absl::holds_alternative<RemoteConsentResolutionData>(value_)) {
    return CACHE_STATUS_REMOTE_CONSENT;
  } else if (absl::holds_alternative<std::string>(value_)) {
    return CACHE_STATUS_REMOTE_CONSENT_APPROVED;
  } else if (absl::holds_alternative<TokenValue>(value_)) {
    return CACHE_STATUS_TOKEN;
  } else {
    DCHECK(absl::holds_alternative<absl::monostate>(value_));
    return CACHE_STATUS_NOTFOUND;
  }
}

bool IdentityTokenCacheValue::is_expired() const {
  return GetStatusInternal() == CACHE_STATUS_NOTFOUND ||
         expiration_time_ < base::Time::Now();
}

const base::Time& IdentityTokenCacheValue::expiration_time() const {
  return expiration_time_;
}

const RemoteConsentResolutionData& IdentityTokenCacheValue::resolution_data()
    const {
  return absl::get<RemoteConsentResolutionData>(value_);
}

const std::string& IdentityTokenCacheValue::consent_result() const {
  return absl::get<std::string>(value_);
}

const std::string& IdentityTokenCacheValue::token() const {
  return absl::get<TokenValue>(value_).token;
}

const std::set<std::string>& IdentityTokenCacheValue::granted_scopes() const {
  return absl::get<TokenValue>(value_).granted_scopes;
}

IdentityTokenCacheValue::TokenValue::TokenValue(
    const std::string& input_token,
    const std::set<std::string>& input_granted_scopes)
    : token(input_token), granted_scopes(input_granted_scopes) {}

IdentityTokenCacheValue::TokenValue::TokenValue(const TokenValue& other) =
    default;
IdentityTokenCacheValue::TokenValue&
IdentityTokenCacheValue::TokenValue::operator=(const TokenValue& other) =
    default;
IdentityTokenCacheValue::TokenValue::~TokenValue() = default;

IdentityTokenCache::AccessTokensKey::AccessTokensKey(
    const ExtensionTokenKey& key)
    : extension_id(key.extension_id), account_id(key.account_info.account_id) {}

IdentityTokenCache::AccessTokensKey::AccessTokensKey(
    const std::string& extension_id,
    const CoreAccountId& account_id)
    : extension_id(extension_id), account_id(account_id) {}

bool IdentityTokenCache::AccessTokensKey::operator<(
    const AccessTokensKey& rhs) const {
  return std::tie(extension_id, account_id) <
         std::tie(rhs.extension_id, rhs.account_id);
}

// Ensure that the access tokens are ordered by scope sizes.
bool IdentityTokenCache::ScopesSizeCompare::operator()(
    const IdentityTokenCacheValue& lhs,
    const IdentityTokenCacheValue& rhs) const {
  std::size_t lhs_size = lhs.granted_scopes().size();
  std::size_t rhs_size = rhs.granted_scopes().size();
  return std::tie(lhs_size, lhs.granted_scopes()) <
         std::tie(rhs_size, rhs.granted_scopes());
}

IdentityTokenCache::IdentityTokenCache() = default;
IdentityTokenCache::~IdentityTokenCache() = default;

void IdentityTokenCache::SetToken(const ExtensionTokenKey& key,
                                  const IdentityTokenCacheValue& token_data) {
  if (token_data.status() == IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND)
    return;

  if (token_data.status() != IdentityTokenCacheValue::CACHE_STATUS_TOKEN) {
    const IdentityTokenCacheValue& cached_value = GetToken(key);
    if (cached_value.status() <= token_data.status()) {
      intermediate_value_cache_.erase(key);
      intermediate_value_cache_.insert(std::make_pair(key, token_data));
    }
  } else {
    // Access tokens are stored in their own cache for subset matching.
    DCHECK(!token_data.granted_scopes().empty());
    intermediate_value_cache_.erase(key);

    AccessTokensKey access_tokens_key(key);
    auto [it, inserted] = access_tokens_cache_.try_emplace(access_tokens_key);

    AccessTokensValue& cached_tokens = it->second;
    // If a cached tokens set already exists, remove any existing token with the
    // same set of scopes.
    cached_tokens.erase(token_data);
    cached_tokens.insert(token_data);
  }
}

void IdentityTokenCache::EraseAccessToken(const std::string& extension_id,
                                          const std::string& token) {
  for (auto entry_it = access_tokens_cache_.begin();
       entry_it != access_tokens_cache_.end(); entry_it++) {
    if (entry_it->first.extension_id == extension_id) {
      AccessTokensValue& cached_tokens = entry_it->second;
      size_t num_erased = std::erase_if(
          cached_tokens, [&token](const IdentityTokenCacheValue& cached_token) {
            return cached_token.token() == token;
          });
      if (num_erased > 0) {
        if (cached_tokens.size() == 0)
          access_tokens_cache_.erase(entry_it);
        // A token is in the cache at most once, so stop searching if erased.
        return;
      }
    }
  }
}

void IdentityTokenCache::EraseAllTokensForExtension(
    const std::string& extension_id) {
  std::erase_if(access_tokens_cache_,
                [&extension_id](const auto& key_value_pair) {
                  const AccessTokensKey& key = key_value_pair.first;
                  return key.extension_id == extension_id;
                });
  std::erase_if(intermediate_value_cache_,
                [&extension_id](const auto& key_value_pair) {
                  const ExtensionTokenKey& key = key_value_pair.first;
                  return key.extension_id == extension_id;
                });
}

void IdentityTokenCache::EraseAllTokens() {
  intermediate_value_cache_.clear();
  access_tokens_cache_.clear();
}

const IdentityTokenCacheValue& IdentityTokenCache::GetToken(
    const ExtensionTokenKey& key) {
  EraseStaleTokens();
  AccessTokensKey access_tokens_key(key);
  auto find_tokens_it = access_tokens_cache_.find(access_tokens_key);
  if (find_tokens_it != access_tokens_cache_.end()) {
    const AccessTokensValue& cached_tokens = find_tokens_it->second;
    auto matched_token_it =
        base::ranges::find_if(cached_tokens, [&key](const auto& cached_token) {
          return key.scopes.size() <= cached_token.granted_scopes().size() &&
                 base::ranges::includes(cached_token.granted_scopes(),
                                        key.scopes);
        });

    if (matched_token_it != cached_tokens.end()) {
      IdentityTokenCacheValue::CacheValueStatus status =
          matched_token_it->status();
      DCHECK(status == IdentityTokenCacheValue::CACHE_STATUS_TOKEN ||
             status == IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND);
      return *matched_token_it;
    }
  }

  const IdentityTokenCacheValue& intermediate_value =
      intermediate_value_cache_[key];
  DCHECK_NE(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            intermediate_value.status());
  return intermediate_value;
}

const IdentityTokenCache::AccessTokensCache&
IdentityTokenCache::access_tokens_cache() {
  return access_tokens_cache_;
}

void IdentityTokenCache::EraseStaleTokens() {
  // Expired tokens have CACHE_STATUS_NOTFOUND status.
  for (auto it = access_tokens_cache_.begin();
       it != access_tokens_cache_.end();) {
    auto& cached_tokens = it->second;
    std::erase_if(cached_tokens, [](const IdentityTokenCacheValue& value) {
      return value.status() == IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND;
    });

    if (cached_tokens.empty())
      it = access_tokens_cache_.erase(it);
    else
      ++it;
  }

  std::erase_if(intermediate_value_cache_, [](const auto& key_value_pair) {
    const IdentityTokenCacheValue& value = key_value_pair.second;
    return value.status() == IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND;
  });
}

}  // namespace extensions
