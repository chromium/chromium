// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/entra_provider_android.h"

#include <map>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/metrics/histogram_tester.h"
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
using Status = enterprise_auth::EntraProviderAndroid::Status;
using AuthenticationResult = EntraProviderAndroid::AuthenticationResult;

void MockJavaReadTokens(
    Status status,
    std::string result,
    base::OnceCallback<void(Status, std::string)> callback) {
  std::move(callback).Run(status, std::move(result));
}

}  // namespace

class EntraProviderAndroidTest : public testing::Test {
 protected:
  // Helper to trigger GetData and block until the callback is run.
  net::HttpRequestHeaders FetchHeaders(Status status,
                                       const std::string& json = "") {
    provider_.SetMockJavaReadTokensForTesting(
        base::BindRepeating(&MockJavaReadTokens, status, std::string(json)));

    base::test::TestFuture<net::HttpRequestHeaders> get_headers_future;
    provider_.GetData(GURL("https://www.foo.bar"),
                      get_headers_future.GetCallback());
    return get_headers_future.Get();
  }

  void ExpectMetrics(AuthenticationResult result,
                     std::optional<Status> failure_reason) {
    histogram_tester_.ExpectUniqueSample(
        EntraProviderAndroid::kAuthenticationResultHistogram, result, 1);
    if (result == AuthenticationResult::kSuccessWithHeaders ||
        result == AuthenticationResult::kSuccessWithNoHeaders) {
      histogram_tester_.ExpectTotalCount(
          EntraProviderAndroid::kDurationSuccessHistogram, 1);
    } else if (result == AuthenticationResult::kFailure) {
      histogram_tester_.ExpectTotalCount(
          EntraProviderAndroid::kDurationFailureHistogram, 1);
      histogram_tester_.ExpectUniqueSample(
          EntraProviderAndroid::kFailureReasonHistogram, failure_reason.value(),
          1);
    }
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
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
  const net::HttpRequestHeaders headers = FetchHeaders(Status::kOk, R"({
    "headers": {
      "x-ms-refreshtokencredential": "first_value",
      "x-ms-refreshtokencredential": "second_value"
    }
  })");

  EXPECT_EQ(headers.GetHeaderVector().size(), 1u);
  EXPECT_TRUE(headers.HasHeader("x-ms-refreshtokencredential"));

  histogram_tester_.ExpectUniqueSample(
      EntraProviderAndroid::kAuthenticationResultHistogram,
      AuthenticationResult::kSuccessWithHeaders, 1);
  histogram_tester_.ExpectTotalCount(
      EntraProviderAndroid::kDurationSuccessHistogram, 1);
}

TEST_F(EntraProviderAndroidTest, GetData_SsoDisabledShortCircuit) {
  // 1. Trigger an error that sets sso_disabled_ = true.
  FetchHeaders(Status::kNoBrokerRegistered);

  histogram_tester_.ExpectUniqueSample(
      EntraProviderAndroid::kAuthenticationResultHistogram,
      AuthenticationResult::kNoBrokerRegistered, 1);
  histogram_tester_.ExpectTotalCount(
      EntraProviderAndroid::kDurationNoBrokerHistogram, 1);

  // 2. Attempt a valid call. Because sso_disabled_ is true, this
  // should early-exit and return empty headers without evaluating
  // the override.
  const net::HttpRequestHeaders headers =
      FetchHeaders(Status::kOk, kValidHeadersJSON);

  EXPECT_TRUE(headers.GetHeaderVector().empty());
  histogram_tester_.ExpectTotalCount(
      EntraProviderAndroid::kAuthenticationResultHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      EntraProviderAndroid::kDurationNoBrokerHistogram, 1);
}

struct SsoDisableTestParams {
  Status status;
  std::string raw_json;
  bool should_disable;
  AuthenticationResult result;
  std::optional<Status> failure_reason;
};

class SsoDisabledTest
    : public EntraProviderAndroidTest,
      public testing::WithParamInterface<SsoDisableTestParams> {};

TEST_P(SsoDisabledTest, SsoDisabledTest) {
  const SsoDisableTestParams& params = GetParam();

  // 1. Trigger an error that sets sso_disabled_ = true.
  const net::HttpRequestHeaders first_headers =
      FetchHeaders(params.status, params.raw_json);

  ExpectMetrics(params.result, params.failure_reason);

  // 2. Attempt a valid call. Because sso_disabled_ is true, this
  // should early-exit and return empty headers without evaluating
  // the override.
  const net::HttpRequestHeaders headers =
      FetchHeaders(Status::kOk, kValidHeadersJSON);

  if (params.should_disable) {
    ExpectMetrics(params.result, params.failure_reason);
    EXPECT_TRUE(headers.GetHeaderVector().empty());
  } else {
    histogram_tester_.ExpectTotalCount(
        EntraProviderAndroid::kAuthenticationResultHistogram, 2);
    EXPECT_FALSE(headers.GetHeaderVector().empty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    SsoDisabledTest,
    SsoDisabledTest,
    testing::Values(
        // Case 0: Token read OK, Valid JSON -> Success
        SsoDisableTestParams{
            .status = Status::kOk,
            .raw_json = "{\"headers\": {\"x-ms-refreshtokencredential\": "
                        "\"valid_header\"}}",
            .should_disable = false,
            .result = AuthenticationResult::kSuccessWithHeaders,
        },

        // Case 1: Token read OK, Empty JSON -> JSON Parsing Failed
        SsoDisableTestParams{
            .status = Status::kOk,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kJsonParsingFailed,
        },

        // Case 2: Token read OK, Invalid header value type -> All Headers
        // Skipped
        SsoDisableTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": 12345,
                  }
                })",
            .should_disable = false,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kAllHeadersSkipped,
        },

        // Case 3: Token read OK, Invalid header key name -> All Headers Skipped
        SsoDisableTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "refreshtokencredential": "valid_header",
                  }
                })",
            .should_disable = false,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kAllHeadersSkipped,
        },

        // Case 4: Token read OK, Malformed header key -> All Headers Skipped
        SsoDisableTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refresht>okencredential": "valid_header",
                  }
                })",
            .should_disable = false,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kAllHeadersSkipped,
        },

        // Case 5: Token read OK, Empty JSON, should_disable true -> JSON
        // Parsing Failed (disables SSO)
        SsoDisableTestParams{
            .status = Status::kOk,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kJsonParsingFailed,
        },

        // Case 6: NoBrokerRegistered -> NoBrokerRegistered (disables SSO)
        SsoDisableTestParams{
            .status = Status::kNoBrokerRegistered,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kNoBrokerRegistered,
        },

        // Case 7: SignatureVerificationFailed -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kSignatureVerificationFailed,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kSignatureVerificationFailed,
        },

        // Case 8: UnexpectedError -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kUnexpectedError,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kUnexpectedError,
        },

        // Case 9: InvalidBundleFormat -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kInvalidBundleFormat,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kInvalidBundleFormat,
        },

        // Case 10: NoBundleResult -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kNoBundleResult,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kNoBundleResult,
        },

        // Case 11: BundleResultContainsEntraError -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kBundleResultContainsEntraError,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kBundleResultContainsEntraError,
        },

        // Case 12: BundleResultContainsOsError -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kBundleResultContainsOsError,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kBundleResultContainsOsError,
        },

        // Case 13: UnexpectedPackageProvider -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kUnexpectedPackageProvider,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kUnexpectedPackageProvider,
        },

        // Case 14: DisallowedDebugPackageProvider -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kDisallowedDebugPackageProvider,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kDisallowedDebugPackageProvider,
        },

        // Case 15: Timeout -> Failure (disables SSO)
        SsoDisableTestParams{
            .status = Status::kTimeout,
            .raw_json = "",
            .should_disable = true,
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kTimeout,
        }));

struct HeadersParsingTestParams {
  Status status;
  std::string raw_json;
  net::HttpRequestHeaders::HeaderVector expected_headers;
  AuthenticationResult result;
  std::optional<Status> failure_reason;
  std::map<EntraProviderAndroid::HeaderSkipReason, int> expected_skip_counts;
};

class HeadersParsingTest
    : public EntraProviderAndroidTest,
      public testing::WithParamInterface<HeadersParsingTestParams> {};

TEST_P(HeadersParsingTest, ParseHeaders) {
  const HeadersParsingTestParams& params = GetParam();
  const net::HttpRequestHeaders headers =
      FetchHeaders(params.status, params.raw_json);

  ExpectMetrics(params.result, params.failure_reason);

  for (const auto& [reason, count] : params.expected_skip_counts) {
    histogram_tester_.ExpectBucketCount(
        EntraProviderAndroid::kHeaderSkipReasonHistogram, reason, count);
  }

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
            .status = Status::kNoBrokerRegistered,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kNoBrokerRegistered,
        },

        HeadersParsingTestParams{
            .status = Status::kSignatureVerificationFailed,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kSignatureVerificationFailed,
        },

        HeadersParsingTestParams{
            .status = Status::kUnexpectedError,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kUnexpectedError,
        },

        HeadersParsingTestParams{
            .status = Status::kInvalidBundleFormat,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kInvalidBundleFormat,
        },

        HeadersParsingTestParams{
            .status = Status::kNoBundleResult,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kNoBundleResult,
        },

        HeadersParsingTestParams{
            .status = Status::kBundleResultContainsEntraError,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kBundleResultContainsEntraError,
        },

        HeadersParsingTestParams{
            .status = Status::kBundleResultContainsOsError,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kBundleResultContainsOsError,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kJsonParsingFailed,
        },

        HeadersParsingTestParams{
            .status = Status::kUnexpectedPackageProvider,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kUnexpectedPackageProvider,
        },

        HeadersParsingTestParams{
            .status = Status::kDisallowedDebugPackageProvider,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kDisallowedDebugPackageProvider,
        },

        HeadersParsingTestParams{
            .status = Status::kTimeout,
            .raw_json = "",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kTimeout,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json =
                "[\"headers\": {\"x-ms-refreshtokencredential\": \"val\"}]",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kJsonParsingFailed,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json =
                "{\"headers\": {\"x-ms-refreshtokencredential\": \"val\"",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kJsonParsingFailed,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = "{\"x-ms-refreshtokencredential\": \"value\"}",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kJsonParsingFailed,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = "{\"headers\": \"value\"}",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kJsonParsingFailed,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = "{\"headers\": {\"x-ms-refreshtokencredential\": "
                        "\"token123\"}}",
            .expected_headers = {{"x-ms-refreshtokencredential", "token123"}},
            .result = AuthenticationResult::kSuccessWithHeaders,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": "token1",
                    "x-ms-devicecredential": "token2"
                  }
                })",
            .expected_headers = {{"x-ms-refreshtokencredential", "token1"},
                                 {"x-ms-devicecredential", "token2"}},
            .result = AuthenticationResult::kSuccessWithHeaders,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": "valid_token",
                    "invalid-key": "some_value"
                  }
                })",
            .expected_headers = {{"x-ms-refreshtokencredential",
                                  "valid_token"}},
            .result = AuthenticationResult::kSuccessWithHeaders,
            .expected_skip_counts =
                {{EntraProviderAndroid::HeaderSkipReason::kNamePrefixMismatch,
                  1}},
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": 12345,
                    "x-ms-devicecredential": "valid_token"
                  }
                })",
            .expected_headers = {{"x-ms-devicecredential", "valid_token"}},
            .result = AuthenticationResult::kSuccessWithHeaders,
            .expected_skip_counts =
                {{EntraProviderAndroid::HeaderSkipReason::kInvalidValueFormat,
                  1}},
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-ms-refreshtokencredential": "invalid\nvalue",
                    "x-ms-devicecredential": "valid_token"
                  }
                })",
            .expected_headers = {{"x-ms-devicecredential", "valid_token"}},
            .result = AuthenticationResult::kSuccessWithHeaders,
            .expected_skip_counts =
                {{EntraProviderAndroid::HeaderSkipReason::kInvalidHeaderValue,
                  1}},
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "X-Ms-Refresh>Credential": "first_value",
                  }
                })",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kAllHeadersSkipped,
            .expected_skip_counts =
                {{EntraProviderAndroid::HeaderSkipReason::kInvalidHeaderName,
                  1}},
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "x-sm-refreshcredential": "first_value",
                  }
                })",
            .expected_headers = {},
            .result = AuthenticationResult::kFailure,
            .failure_reason = Status::kAllHeadersSkipped,
            .expected_skip_counts =
                {{EntraProviderAndroid::HeaderSkipReason::kNamePrefixMismatch,
                  1}},
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
            .raw_json = R"({
                  "headers": {
                    "X-Ms-RefreshCredential": "token",
                  }
                })",
            .expected_headers = {{"X-Ms-RefreshCredential", "token"}},
            .result = AuthenticationResult::kSuccessWithHeaders,
        },

        HeadersParsingTestParams{
            .status = Status::kOk,
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
                                 {"x-ms-devicecredential5", "token11"}},
            .result = AuthenticationResult::kSuccessWithHeaders,
            .expected_skip_counts =
                {{EntraProviderAndroid::HeaderSkipReason::kNamePrefixMismatch,
                  2}},
        }));
