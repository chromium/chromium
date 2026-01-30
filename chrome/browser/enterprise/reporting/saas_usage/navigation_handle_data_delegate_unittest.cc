// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/navigation_handle_data_delegate.h"

#include "content/public/test/mock_navigation_handle.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

class NavigationHandleDataDelegateTest : public testing::Test {};

TEST_F(NavigationHandleDataDelegateTest, GetUrl) {
  const GURL url("https://example.com/");
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(url);

  NavigationHandleDataDelegate delegate(navigation_handle);
  EXPECT_EQ(url, delegate.GetUrl());
}

TEST_F(NavigationHandleDataDelegateTest, GetEncryptionProtocol_NoSslInfo) {
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("http://example.com/"));

  NavigationHandleDataDelegate delegate(navigation_handle);
  EXPECT_EQ("Unencrypted", delegate.GetEncryptionProtocol());
}

struct EncryptionProtocolTestParam {
  const int ssl_version;
  const char* const expected_protocol;
};

class NavigationHandleDataDelegateEncryptionTest
    : public NavigationHandleDataDelegateTest,
      public testing::WithParamInterface<EncryptionProtocolTestParam> {};

TEST_P(NavigationHandleDataDelegateEncryptionTest, GetEncryptionProtocol) {
  auto param = GetParam();
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://example.com/"));
  net::SSLInfo ssl_info;
  net::SSLConnectionStatusSetVersion(param.ssl_version,
                                     &ssl_info.connection_status);
  navigation_handle.set_ssl_info(ssl_info);

  NavigationHandleDataDelegate delegate(navigation_handle);
  EXPECT_EQ(param.expected_protocol, delegate.GetEncryptionProtocol());
}

INSTANTIATE_TEST_SUITE_P(All,
                         NavigationHandleDataDelegateEncryptionTest,
                         testing::Values(
                             EncryptionProtocolTestParam{
                                 net::SSL_CONNECTION_VERSION_TLS1_3, "TLS 1.3"},
                             EncryptionProtocolTestParam{
                                 net::SSL_CONNECTION_VERSION_TLS1_2, "TLS 1.2"},
                             EncryptionProtocolTestParam{
                                 net::SSL_CONNECTION_VERSION_TLS1_1, "TLS 1.1"},
                             EncryptionProtocolTestParam{
                                 net::SSL_CONNECTION_VERSION_TLS1, "TLS 1.0"},
                             EncryptionProtocolTestParam{
                                 net::SSL_CONNECTION_VERSION_QUIC, "QUIC"}));

}  // namespace enterprise_reporting
