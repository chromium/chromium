// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/entra_provider_android.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

constexpr char kValidHeadersJSON[] =
    "{\"headers\": {\"x-ms-refreshtokencredential\": "
    "\"token123\"}}";

using enterprise_auth::EntraProviderAndroid;

void MockJavaReadTokens(
    EntraProviderAndroid::TokenReadResult result_code,
    std::string result,
    base::OnceCallback<void(EntraProviderAndroid::TokenReadResult, std::string)>
        callback) {
  std::move(callback).Run(result_code, std::move(result));
}

}  // namespace

class EntraProviderAndroidTest : public testing::Test {
 protected:
  // Helper to trigger GetData and block until the callback is run.
  net::HttpRequestHeaders FetchHeaders(
      EntraProviderAndroid::TokenReadResult result_code,
      const std::string& json = "") {
    provider_.SetMockJavaReadTokensForTesting(base::BindRepeating(
        &MockJavaReadTokens, result_code, std::string(json)));

    base::test::TestFuture<net::HttpRequestHeaders> get_headers_future;
    provider_.GetData(GURL("https://www.foo.bar"),
                      get_headers_future.GetCallback());
    return get_headers_future.Get();
  }

  base::test::TaskEnvironment task_environment_;
  EntraProviderAndroid provider_;
};

TEST_F(EntraProviderAndroidTest, SupportsOriginFiltering) {
  EXPECT_TRUE(provider_.SupportsOriginFiltering());
}

TEST_F(EntraProviderAndroidTest, FetchOrigins_ReturnsValidOrigins) {
  base::test::TestFuture<std::unique_ptr<std::vector<url::Origin>>>
      origins_future;
  provider_.FetchOrigins(origins_future.GetCallback());
  std::unique_ptr<std::vector<url::Origin>> origins = origins_future.Take();
  ASSERT_TRUE(origins);
  ASSERT_FALSE(origins->empty());
  for (const auto& origin : *origins) {
    EXPECT_FALSE(origin.Serialize().empty());
  }
}

TEST_F(EntraProviderAndroidTest, GetData_DuplicateHeaderKey) {
  const net::HttpRequestHeaders headers = FetchHeaders(
      enterprise_auth::EntraProviderAndroid::TokenReadResult::kOk, R"({
    "headers": {
      "x-ms-refreshtokencredential": "first_value",
      "x-ms-refreshtokencredential": "second_value"
    }
  })");

  EXPECT_EQ(headers.GetHeaderVector().size(), 1u);
  EXPECT_TRUE(headers.HasHeader("x-ms-refreshtokencredential"));
}

TEST_F(EntraProviderAndroidTest, GetData_SsoDisabledShortCircuit) {
  // 1. Trigger an error that sets sso_disabled_ = true.
  FetchHeaders(enterprise_auth::EntraProviderAndroid::TokenReadResult::
                   kNoBrokerRegistered);

  // 2. Attempt a valid call. Because sso_disabled_ is true, this
  // should early-exit and return empty headers without evaluating
  // the override.
  const net::HttpRequestHeaders headers =
      FetchHeaders(enterprise_auth::EntraProviderAndroid::TokenReadResult::kOk,
                   kValidHeadersJSON);

  EXPECT_TRUE(headers.GetHeaderVector().empty());
}

struct SsoDisableTestParams {
  EntraProviderAndroid::TokenReadResult token_read_result;
  std::string raw_json;
  bool should_disable;
};

class SsoDisabledTest
    : public EntraProviderAndroidTest,
      public testing::WithParamInterface<SsoDisableTestParams> {};

TEST_P(SsoDisabledTest, SsoDisabledTest) {
  const SsoDisableTestParams& params = GetParam();

  // 1. Trigger an error that sets sso_disabled_ = true.
  FetchHeaders(params.token_read_result, params.raw_json);

  // 2. Attempt a valid call. Because sso_disabled_ is true, this
  // should early-exit and return empty headers without evaluating
  // the override.
  const net::HttpRequestHeaders headers =
      FetchHeaders(enterprise_auth::EntraProviderAndroid::TokenReadResult::kOk,
                   kValidHeadersJSON);

  if (params.should_disable) {
    EXPECT_TRUE(headers.GetHeaderVector().empty());
  } else {
    EXPECT_FALSE(headers.GetHeaderVector().empty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    SsoDisabledTest,
    SsoDisabledTest,
    testing::Values(
        SsoDisableTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = kValidHeadersJSON,
            .should_disable = false},

        SsoDisableTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = "",
            .should_disable = true},

        SsoDisableTestParams{
            .token_read_result =
                EntraProviderAndroid::TokenReadResult::kNoBrokerRegistered,
            .raw_json = kValidHeadersJSON,
            .should_disable = true},

        SsoDisableTestParams{.token_read_result = EntraProviderAndroid::
                                 TokenReadResult::kSignatureVerificationFailed,
                             .raw_json = kValidHeadersJSON,
                             .should_disable = true},

        SsoDisableTestParams{
            .token_read_result =
                EntraProviderAndroid::TokenReadResult::kUnexpectedError,
            .raw_json = kValidHeadersJSON,
            .should_disable = true},

        SsoDisableTestParams{
            .token_read_result =
                EntraProviderAndroid::TokenReadResult::kInvalidBundleFormat,
            .raw_json = kValidHeadersJSON,
            .should_disable = true},

        SsoDisableTestParams{
            .token_read_result =
                EntraProviderAndroid::TokenReadResult::kNoBundleResult,
            .raw_json = kValidHeadersJSON,
            .should_disable = true},

        SsoDisableTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::
                kBundleResultContainsEntraError,
            .raw_json = kValidHeadersJSON,
            .should_disable = true},

        SsoDisableTestParams{.token_read_result = EntraProviderAndroid::
                                 TokenReadResult::kBundleResultContainsOsError,
                             .raw_json = kValidHeadersJSON,
                             .should_disable = true}));

struct HeadersParsingTestParams {
  EntraProviderAndroid::TokenReadResult token_read_result;
  std::string raw_json;
  net::HttpRequestHeaders::HeaderVector expected_headers;
};

class HeadersParsingTest
    : public EntraProviderAndroidTest,
      public testing::WithParamInterface<HeadersParsingTestParams> {};

TEST_P(HeadersParsingTest, ParseHeaders) {
  const HeadersParsingTestParams& params = GetParam();
  const net::HttpRequestHeaders headers =
      FetchHeaders(params.token_read_result, params.raw_json);
  const auto& headers_vector = headers.GetHeaderVector();
  EXPECT_EQ(params.expected_headers.size(), headers_vector.size());
  for (const auto& key_value_pair : params.expected_headers) {
    auto itr =
        std::find(headers_vector.begin(), headers_vector.end(), key_value_pair);
    EXPECT_NE(itr, headers_vector.end());
  }
}

INSTANTIATE_TEST_SUITE_P(
    HeadersParsingTest,
    HeadersParsingTest,
    testing::Values(

        HeadersParsingTestParams{
            .token_read_result =
                EntraProviderAndroid::TokenReadResult::kNoBrokerRegistered,
            .raw_json = "",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::
                kSignatureVerificationFailed,
            .raw_json = "",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result =
                EntraProviderAndroid::TokenReadResult::kUnexpectedError,
            .raw_json = "",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result =
                EntraProviderAndroid::TokenReadResult::kInvalidBundleFormat,
            .raw_json = "",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result =
                EntraProviderAndroid::TokenReadResult::kNoBundleResult,
            .raw_json = "",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::
                kBundleResultContainsEntraError,
            .raw_json = "",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::
                kBundleResultContainsOsError,
            .raw_json = "",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = "",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json =
                "[\"headers\": {\"x-ms-refreshtokencredential\": \"val\"}]",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json =
                "{\"headers\": {\"x-ms-refreshtokencredential\": \"val\"",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = "{\"x-ms-refreshtokencredential\": \"value\"}",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = "{\"headers\": \"value\"}",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = "{\"headers\": {\"x-ms-refreshtokencredential\": "
                        "\"token123\"}}",
            .expected_headers = {{"x-ms-refreshtokencredential", "token123"}}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": "token1",
                    "x-ms-devicecredential": "token2"
                  }
                })",
            .expected_headers = {{"x-ms-refreshtokencredential", "token1"},
                                 {"x-ms-devicecredential", "token2"}}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": "valid_token",
                    "invalid-key": "some_value"
                  }
                })",
            .expected_headers = {{"x-ms-refreshtokencredential",
                                  "valid_token"}}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": 12345,
                    "x-ms-devicecredential": "valid_token"
                  }
                })",
            .expected_headers = {{"x-ms-devicecredential", "valid_token"}}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": "invalid\nvalue",
                    "x-ms-devicecredential": "valid_token"
                  }
                })",
            .expected_headers = {{"x-ms-devicecredential", "valid_token"}}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = R"({
                  "headers": {
                    "X-Ms-Refresh>Credential": "first_value",
                  }
                })",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-sm-refreshcredential": "first_value",
                  }
                })",
            .expected_headers = {}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = R"({
                  "headers": {
                    "X-Ms-RefreshCredential": "token",
                  }
                })",
            .expected_headers = {{"X-Ms-RefreshCredential", "token"}}},

        HeadersParsingTestParams{
            .token_read_result = EntraProviderAndroid::TokenReadResult::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": "token0",
                    "x-ms-refreshtokencredential1": "token1",
                    "x-ms-refreshtokencredential2": "token2",
                    "x-ms-refreshtokencredential3": "token3",
                    "x-ms-refreshtokencredential4": "token4",
                    "x-ms-refreshtokencredential5": "token5",
                    "x-ms-devicecredential": "token6",
                    "x-ms-devicecredential1": "token7",
                    "x-ms-devicecredential2": "token8",
                    "x-ms-devicecredential3": "token9",
                    "x-ms-devicecredential4": "token10",
                    "x-ms-devicecredential5": "token11",
                    "invalid-key": "some_value",
                    "random-header": "another_value"
                  },
                  "foo": "bar",
                  "nested": {"abc": 123}
                })",
            .expected_headers = {{"x-ms-refreshtokencredential", "token0"},
                                 {"x-ms-refreshtokencredential1", "token1"},
                                 {"x-ms-refreshtokencredential2", "token2"},
                                 {"x-ms-refreshtokencredential3", "token3"},
                                 {"x-ms-refreshtokencredential4", "token4"},
                                 {"x-ms-refreshtokencredential5", "token5"},
                                 {"x-ms-devicecredential", "token6"},
                                 {"x-ms-devicecredential1", "token7"},
                                 {"x-ms-devicecredential2", "token8"},
                                 {"x-ms-devicecredential3", "token9"},
                                 {"x-ms-devicecredential4", "token10"},
                                 {"x-ms-devicecredential5", "token11"}}}));
