// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/google_services_connectivity_routine_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash::network_diagnostics {

namespace {

using ConnectivityResultCode =
    hosts_connectivity_diagnostics::ConnectivityResultCode;
using ProblemType =
    chromeos::network_diagnostics::mojom::GoogleServicesConnectivityProblemType;
using ProxyProblemType = chromeos::network_diagnostics::mojom::
    GoogleServicesConnectivityProxyProblemType;

constexpr ConnectivityResultCode kAllResultCodes[] = {
    ConnectivityResultCode::UNSPECIFIED,
    ConnectivityResultCode::SUCCESS,
    ConnectivityResultCode::INTERNAL_ERROR,
    ConnectivityResultCode::NO_VALID_HOSTNAME,
    ConnectivityResultCode::UNKNOWN_ERROR,
    ConnectivityResultCode::CONNECTION_FAILURE,
    ConnectivityResultCode::CONNECTION_TIMEOUT,
    ConnectivityResultCode::DNS_RESOLUTION_ERROR,
    ConnectivityResultCode::PROXY_DNS_RESOLUTION_ERROR,
    ConnectivityResultCode::PROXY_CONNECTION_FAILURE,
    ConnectivityResultCode::SSL_CONNECTION_ERROR,
    ConnectivityResultCode::PEER_CERTIFICATE_ERROR,
    ConnectivityResultCode::HTTP_ERROR,
    ConnectivityResultCode::NO_NETWORK_ERROR,
    ConnectivityResultCode::NO_VALID_PROXY,
};

}  // namespace

TEST(GoogleServicesConnectivityRoutineUtilTest, ToOptionalStrIfNonEmpty) {
  EXPECT_EQ(std::nullopt, ToOptionalStrIfNonEmpty(""));
  EXPECT_EQ("test", ToOptionalStrIfNonEmpty("test"));
}

TEST(GoogleServicesConnectivityRoutineUtilTest,
     IsSuccessfulOrUnspecifiedResult) {
  for (ConnectivityResultCode code : kAllResultCodes) {
    const bool expected = (code == ConnectivityResultCode::SUCCESS ||
                           code == ConnectivityResultCode::UNSPECIFIED);
    EXPECT_EQ(expected, IsSuccessfulOrUnspecifiedResult(code));
  }
}

TEST(GoogleServicesConnectivityRoutineUtilTest, IsProxyConnectionError) {
  for (ConnectivityResultCode code : kAllResultCodes) {
    const bool expected =
        (code == ConnectivityResultCode::PROXY_DNS_RESOLUTION_ERROR ||
         code == ConnectivityResultCode::PROXY_CONNECTION_FAILURE);
    EXPECT_EQ(expected, IsProxyConnectionError(code));
  }
}

TEST(GoogleServicesConnectivityRoutineUtilTest, IsNoValidProxyError) {
  for (ConnectivityResultCode code : kAllResultCodes) {
    const bool expected = (code == ConnectivityResultCode::NO_VALID_PROXY);
    EXPECT_EQ(expected, IsNoValidProxyError(code));
  }
}

TEST(GoogleServicesConnectivityRoutineUtilTest, ToConnectivityProblemType) {
  const struct {
    ConnectivityResultCode code;
    ProblemType expected;
  } kMappings[] = {
      {ConnectivityResultCode::INTERNAL_ERROR, ProblemType::kInternalError},
      {ConnectivityResultCode::NO_VALID_HOSTNAME, ProblemType::kInternalError},
      {ConnectivityResultCode::UNKNOWN_ERROR, ProblemType::kUnknownError},
      {ConnectivityResultCode::CONNECTION_FAILURE,
       ProblemType::kConnectionFailure},
      {ConnectivityResultCode::CONNECTION_TIMEOUT,
       ProblemType::kConnectionTimeout},
      {ConnectivityResultCode::DNS_RESOLUTION_ERROR,
       ProblemType::kDnsResolutionError},
      {ConnectivityResultCode::SSL_CONNECTION_ERROR,
       ProblemType::kSSLConnectionError},
      {ConnectivityResultCode::PEER_CERTIFICATE_ERROR,
       ProblemType::kPeerCertificateError},
      {ConnectivityResultCode::HTTP_ERROR, ProblemType::kHttpError},
      {ConnectivityResultCode::NO_NETWORK_ERROR, ProblemType::kNoNetworkError},
  };

  for (const auto& [code, expected] : kMappings) {
    EXPECT_EQ(expected, ToConnectivityProblemType(code));
  }

  EXPECT_EQ(
      ProblemType::kInternalError,
      ToConnectivityProblemType(static_cast<ConnectivityResultCode>(999)));
}

TEST(GoogleServicesConnectivityRoutineUtilTest, ToProxyProblemType) {
  EXPECT_EQ(
      ProxyProblemType::kProxyDnsResolutionError,
      ToProxyProblemType(ConnectivityResultCode::PROXY_DNS_RESOLUTION_ERROR));
  EXPECT_EQ(
      ProxyProblemType::kProxyConnectionFailure,
      ToProxyProblemType(ConnectivityResultCode::PROXY_CONNECTION_FAILURE));
}

TEST(GoogleServicesConnectivityRoutineUtilTest,
     ToProxyProblemType_InvalidCodeReturnsFallback) {
  EXPECT_EQ(ProxyProblemType::kProxyConnectionFailure,
            ToProxyProblemType(ConnectivityResultCode::DNS_RESOLUTION_ERROR));
}

}  // namespace ash::network_diagnostics
