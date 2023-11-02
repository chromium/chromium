// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_token_cache.h"

#include <set>
#include <string>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kDefaultExtensionId[] = "ext_id";

}  // namespace

class IdentityTokenCacheTest : public testing::Test {
 public:
  void SetAccessToken(const std::string& ext_id,
                      const std::string& token_string,
                      const std::set<std::string>& scopes) {
    SetAccessTokenInternal(ext_id, token_string, scopes, base::Seconds(3600));
  }

  void SetExpiredAccessToken(const std::string& ext_id,
                             const std::string& token_string,
                             const std::set<std::string>& scopes) {
    // Token must not be expired at the insertion moment.
    SetAccessTokenInternal(ext_id, token_string, scopes, base::Milliseconds(1));
    task_environment_.FastForwardBy(base::Milliseconds(2));
  }

  void SetRemoteConsentApprovedToken(const std::string& ext_id,
                                     const std::string& consent_result,
                                     const std::set<std::string>& scopes) {
    ExtensionTokenKey key(ext_id, CoreAccountInfo(), scopes);
    IdentityTokenCacheValue token =
        IdentityTokenCacheValue::CreateRemoteConsentApproved(consent_result);
    cache_.SetToken(key, token);
  }

  const IdentityTokenCacheValue& GetToken(const std::string& ext_id,
                                          const std::set<std::string>& scopes) {
    ExtensionTokenKey key(ext_id, CoreAccountInfo(), scopes);
    return cache_.GetToken(key);
  }

  IdentityTokenCache& cache() { return cache_; }

 private:
  void SetAccessTokenInternal(const std::string& ext_id,
                              const std::string& token_string,
                              const std::set<std::string>& scopes,
                              base::TimeDelta time_to_live) {
    ExtensionTokenKey key(ext_id, CoreAccountInfo(), scopes);
    IdentityTokenCacheValue token = IdentityTokenCacheValue::CreateToken(
        token_string, scopes, time_to_live);
    cache_.SetToken(key, token);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  IdentityTokenCache cache_;
};

TEST_F(IdentityTokenCacheTest, AccessTokenCacheHit) {
  std::string token_string = "token";
  std::set<std::string> scopes = {"foo", "bar"};
  SetAccessToken(kDefaultExtensionId, token_string, scopes);

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, scopes);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN, cached_token.status());
  EXPECT_EQ(token_string, cached_token.token());
  EXPECT_EQ(scopes, cached_token.granted_scopes());
}

// The cache should return NOTFOUND status when a token expires.
// Regression test for https://crbug.com/1127187.
TEST_F(IdentityTokenCacheTest, ExpiredAccessTokenCacheHit) {
  std::string token_string = "token";
  std::set<std::string> scopes = {"foo", "bar"};
  SetExpiredAccessToken(kDefaultExtensionId, token_string, scopes);

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, scopes);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            cached_token.status());
}

TEST_F(IdentityTokenCacheTest, IntermediateValueCacheHit) {
  std::string consent_result = "result";
  std::set<std::string> scopes = {"foo", "bar"};
  SetRemoteConsentApprovedToken(kDefaultExtensionId, consent_result, scopes);

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, scopes);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT_APPROVED,
            cached_token.status());
  EXPECT_EQ(consent_result, cached_token.consent_result());
}

TEST_F(IdentityTokenCacheTest, CacheHitPriority) {
  std::string token_string = "token";
  std::set<std::string> scopes = {"foo", "bar"};
  SetAccessToken(kDefaultExtensionId, token_string, scopes);
  SetRemoteConsentApprovedToken(kDefaultExtensionId, "result", scopes);

  // Prioritize access tokens over immediate values.
  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, scopes);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN, cached_token.status());
  EXPECT_EQ(token_string, cached_token.token());
  EXPECT_EQ(scopes, cached_token.granted_scopes());
}

TEST_F(IdentityTokenCacheTest, CacheHitAfterExpired) {
  std::string token_string = "token";
  std::set<std::string> scopes = {"foo", "bar"};
  SetExpiredAccessToken(kDefaultExtensionId, token_string, scopes);

  std::string consent_result = "result";
  SetRemoteConsentApprovedToken(kDefaultExtensionId, consent_result, scopes);

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, scopes);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT_APPROVED,
            cached_token.status());
  EXPECT_EQ(consent_result, cached_token.consent_result());
}

TEST_F(IdentityTokenCacheTest, AccessTokenCacheMiss) {
  std::string ext_1 = "ext_1";
  std::set<std::string> scopes_1 = {"foo", "bar"};
  SetAccessToken(ext_1, "token_1", scopes_1);

  std::string ext_2 = "ext_2";
  std::set<std::string> scopes_2 = {"foo", "foobar"};
  SetAccessToken(ext_2, "token_2", scopes_2);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(ext_2, scopes_1).status());
}

TEST_F(IdentityTokenCacheTest, IntermediateValueCacheMiss) {
  std::string ext_1 = "ext_1";
  std::set<std::string> scopes_1 = {"foo", "bar"};
  SetRemoteConsentApprovedToken(ext_1, "result_1", scopes_1);

  std::string ext_2 = "ext_2";
  std::set<std::string> scopes_2 = {"foo", "foobar"};
  SetRemoteConsentApprovedToken(ext_2, "result_2", scopes_2);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(ext_2, scopes_1).status());
}

TEST_F(IdentityTokenCacheTest, EraseAccessToken) {
  std::string token_string = "token";
  std::set<std::string> scopes = {"foo", "bar"};
  SetAccessToken(kDefaultExtensionId, token_string, scopes);

  cache().EraseAccessToken(kDefaultExtensionId, token_string);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(kDefaultExtensionId, scopes).status());
}

TEST_F(IdentityTokenCacheTest, EraseAccessTokenOthersUnaffected) {
  std::string token_string = "token";
  std::set<std::string> scopes = {"foo", "bar"};
  SetAccessToken(kDefaultExtensionId, token_string, scopes);

  std::string unrelated_token_string = "unrelated_token";
  std::set<std::string> unrelated_scopes = {"foo", "foobar"};
  SetAccessToken(kDefaultExtensionId, unrelated_token_string, unrelated_scopes);

  cache().EraseAccessToken(kDefaultExtensionId, token_string);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(kDefaultExtensionId, scopes).status());

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, unrelated_scopes);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN, cached_token.status());
  EXPECT_EQ(unrelated_token_string, cached_token.token());
  EXPECT_EQ(unrelated_scopes, cached_token.granted_scopes());
}

TEST_F(IdentityTokenCacheTest, EraseAllTokens) {
  std::string ext_1 = "ext_1";
  std::string token_string = "token_1";
  std::set<std::string> scopes_1 = {"foo", "bar"};
  SetAccessToken(ext_1, token_string, scopes_1);

  std::string ext_2 = "ext_2";
  std::string remote_consent = "approved";
  std::set<std::string> scopes_2 = {"foo", "foobar"};
  SetRemoteConsentApprovedToken(ext_2, remote_consent, scopes_2);

  cache().EraseAllTokens();

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(ext_1, scopes_1).status());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(ext_2, scopes_2).status());
}

TEST_F(IdentityTokenCacheTest, EraseAllTokensForExtension) {
  std::string token_string = "token";
  std::set<std::string> scopes_1 = {"foo", "bar"};
  SetAccessToken(kDefaultExtensionId, token_string, scopes_1);

  std::string remote_consent = "approved";
  std::set<std::string> scopes_2 = {"foo", "foobar"};
  SetRemoteConsentApprovedToken(kDefaultExtensionId, remote_consent, scopes_2);

  std::string unrelated_extension = "ext_unrelated";
  SetAccessToken(unrelated_extension, token_string, scopes_1);
  SetRemoteConsentApprovedToken(unrelated_extension, remote_consent, scopes_2);

  cache().EraseAllTokensForExtension(kDefaultExtensionId);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(kDefaultExtensionId, scopes_1).status());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(kDefaultExtensionId, scopes_2).status());

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            GetToken(unrelated_extension, scopes_1).status());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT_APPROVED,
            GetToken(unrelated_extension, scopes_2).status());
}

TEST_F(IdentityTokenCacheTest, GetAccessTokens) {
  std::string ext_1 = "ext_1";
  std::string token_string_1 = "token_1";
  std::set<std::string> scopes_1 = {"foo", "bar"};
  SetAccessToken(ext_1, token_string_1, scopes_1);

  std::string ext_2 = "ext_2";
  std::string token_string_2 = "token_2";
  std::set<std::string> scopes_2 = {"foobar"};
  SetAccessToken(ext_2, token_string_2, scopes_2);

  IdentityTokenCache::AccessTokensCache cached_access_tokens =
      cache().access_tokens_cache();
  EXPECT_EQ(2ul, cached_access_tokens.size());

  IdentityTokenCache::AccessTokensKey key_1(ext_1, CoreAccountId());
  const IdentityTokenCache::AccessTokensValue& cached_tokens_1 =
      cached_access_tokens[key_1];
  EXPECT_EQ(1ul, cached_tokens_1.size());

  const IdentityTokenCacheValue& cached_token_1 = *(cached_tokens_1.begin());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            cached_token_1.status());
  EXPECT_EQ(token_string_1, cached_token_1.token());
  EXPECT_EQ(scopes_1, cached_token_1.granted_scopes());

  IdentityTokenCache::AccessTokensKey key_2(ext_2, CoreAccountId());
  const IdentityTokenCache::AccessTokensValue& cached_tokens_2 =
      cached_access_tokens[key_2];
  EXPECT_EQ(1ul, cached_tokens_2.size());

  const IdentityTokenCacheValue& cached_token_2 = *(cached_tokens_2.begin());
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN,
            cached_token_2.status());
  EXPECT_EQ(token_string_2, cached_token_2.token());
  EXPECT_EQ(scopes_2, cached_token_2.granted_scopes());
}

// Newly cached access tokens should override previously cached values with the
// same scopes.
TEST_F(IdentityTokenCacheTest, OverrideAccessToken) {
  std::set<std::string> scopes = {"foo", "bar", "foobar"};
  SetAccessToken(kDefaultExtensionId, "token1", scopes);

  std::string override_token = "token_2";
  SetAccessToken(kDefaultExtensionId, override_token, scopes);
  cache().EraseAccessToken(kDefaultExtensionId, override_token);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(kDefaultExtensionId, scopes).status());
}

TEST_F(IdentityTokenCacheTest, OverrideIntermediateToken) {
  std::set<std::string> scopes = {"foo", "bar", "foobar"};
  SetRemoteConsentApprovedToken(kDefaultExtensionId, "result", scopes);

  std::string override_token = "token";
  SetAccessToken(kDefaultExtensionId, override_token, scopes);
  cache().EraseAccessToken(kDefaultExtensionId, override_token);

  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
            GetToken(kDefaultExtensionId, scopes).status());
}

TEST_F(IdentityTokenCacheTest, SubsetMatchCacheHit) {
  std::set<std::string> superset_scopes = {"foo", "bar"};
  std::string token_string = "token";
  SetAccessToken(kDefaultExtensionId, token_string, superset_scopes);

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, std::set<std::string>({"foo"}));
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN, cached_token.status());
  EXPECT_EQ(token_string, cached_token.token());
  EXPECT_EQ(superset_scopes, cached_token.granted_scopes());
}

TEST_F(IdentityTokenCacheTest, SubsetMatchCacheHitPriority) {
  std::set<std::string> scopes_smallest = {"foo"};
  SetRemoteConsentApprovedToken(kDefaultExtensionId, "result", scopes_smallest);

  std::set<std::string> scopes_large = {"foo", "bar", "foobar"};
  std::string token_string_large = "token_large";
  SetAccessToken(kDefaultExtensionId, token_string_large, scopes_large);

  std::set<std::string> scopes_small = {"foo", "bar"};
  std::string token_string_small = "token_small";
  SetAccessToken(kDefaultExtensionId, token_string_small, scopes_small);

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, std::set<std::string>({"foo"}));
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN, cached_token.status());
  EXPECT_EQ(token_string_small, cached_token.token());
  EXPECT_EQ(scopes_small, cached_token.granted_scopes());
}

TEST_F(IdentityTokenCacheTest, SubsetMatchCacheHitPriorityOneExpired) {
  std::set<std::string> scopes_smallest = {"foo"};
  std::string consent_result = "result";
  SetRemoteConsentApprovedToken(kDefaultExtensionId, consent_result,
                                scopes_smallest);

  std::set<std::string> scopes_small = {"foo", "bar"};
  std::string token_string_small = "token_small";
  SetExpiredAccessToken(kDefaultExtensionId, token_string_small, scopes_small);

  std::set<std::string> scopes_large = {"foo", "bar", "foobar"};
  std::string token_string_large = "token_large";
  SetAccessToken(kDefaultExtensionId, token_string_large, scopes_large);

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, scopes_small);
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_TOKEN, cached_token.status());
  EXPECT_EQ(token_string_large, cached_token.token());
  EXPECT_EQ(scopes_large, cached_token.granted_scopes());
}

TEST_F(IdentityTokenCacheTest, SubsetMatchCacheHitPriorityTwoExpired) {
  std::set<std::string> scopes_smallest = {"foo"};
  std::string consent_result = "result";
  SetRemoteConsentApprovedToken(kDefaultExtensionId, consent_result,
                                scopes_smallest);

  std::set<std::string> scopes_small = {"foo", "bar"};
  std::string token_string_small = "token_small";
  SetExpiredAccessToken(kDefaultExtensionId, token_string_small, scopes_small);

  std::set<std::string> scopes_large = {"foo", "bar", "foobar"};
  std::string token_string_large = "token_large";
  SetExpiredAccessToken(kDefaultExtensionId, token_string_large, scopes_large);

  const IdentityTokenCacheValue& cached_token =
      GetToken(kDefaultExtensionId, std::set<std::string>({"foo"}));
  EXPECT_EQ(IdentityTokenCacheValue::CACHE_STATUS_REMOTE_CONSENT_APPROVED,
            cached_token.status());
  EXPECT_EQ(consent_result, cached_token.consent_result());
}

}  // namespace extensions
