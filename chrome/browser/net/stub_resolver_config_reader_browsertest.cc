// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/stub_resolver_config_reader.h"

#include <string>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/public/secure_dns_mode.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace {

SecureDnsConfig GetSecureDnsConfiguration(
    bool force_check_parental_controls_for_automatic_mode) {
  return SystemNetworkContextManager::GetStubResolverConfigReader()
      ->GetSecureDnsConfiguration(
          force_check_parental_controls_for_automatic_mode);
}

bool GetInsecureStubResolverEnabled() {
  return SystemNetworkContextManager::GetStubResolverConfigReader()
      ->GetInsecureStubResolverEnabled();
}

// A custom matcher to validate a DnsOverHttpsServerConfig instance.
MATCHER_P2(DnsOverHttpsServerConfigMatcher, server_template, use_post, "") {
  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Field(&net::DnsOverHttpsServerConfig::server_template,
                         server_template),
          testing::Field(&net::DnsOverHttpsServerConfig::use_post, use_post)),
      arg, result_listener);
}

class StubResolverConfigReaderBrowsertest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  StubResolverConfigReaderBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(features::kAsyncDns, GetParam());
  }
  ~StubResolverConfigReaderBrowsertest() override = default;

  void SetUpOnMainThread() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(https://crbug.com/1225151): flaky
#if defined(OS_WIN)
#define MAYBE_StubResolverConfig DISABLED_StubResolverConfig
#else
#define MAYBE_StubResolverConfig StubResolverConfig
#endif

// Checks that the values returned by GetStubResolverConfigForTesting() match
// `features::kAsyncDns` (With empty DNS over HTTPS prefs). Then sets various
// DoH modes and DoH template strings and makes sure the settings are respected.
IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest,
                       MAYBE_StubResolverConfig) {
  bool async_dns_feature_enabled = GetParam();

  // Mark as not enterprise managed.
#if defined(OS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
#endif

  // Check initial state.
  SecureDnsConfig secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  if (base::FeatureList::IsEnabled(features::kDnsOverHttps)) {
    EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  } else {
    EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  }
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  std::string good_post_template = "https://foo.test/";
  std::string good_get_template = "https://bar.test/dns-query{?dns}";
  std::string bad_template = "dns-query{?dns}";
  std::string good_then_bad_template = good_get_template + " " + bad_template;
  std::string bad_then_good_template = bad_template + " " + good_get_template;
  std::string multiple_good_templates =
      "  " + good_get_template + "   " + good_post_template + "  ";

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_template);
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kSecure, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_post_template);
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kSecure, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAreArray({
                  DnsOverHttpsServerConfigMatcher(good_post_template, true),
              }));

  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_template);
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_then_bad_template);
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAreArray({
                  DnsOverHttpsServerConfigMatcher(good_get_template, false),
              }));

  local_state->SetString(prefs::kDnsOverHttpsTemplates, bad_then_good_template);
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAreArray({
                  DnsOverHttpsServerConfigMatcher(good_get_template, false),
              }));

  local_state->SetString(prefs::kDnsOverHttpsTemplates,
                         multiple_good_templates);
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(),
              testing::ElementsAreArray({
                  DnsOverHttpsServerConfigMatcher(good_get_template, false),
                  DnsOverHttpsServerConfigMatcher(good_post_template, true),
              }));

  local_state->SetString(prefs::kDnsOverHttpsMode, SecureDnsConfig::kModeOff);
  local_state->SetString(prefs::kDnsOverHttpsTemplates, good_get_template);
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  local_state->SetString(prefs::kDnsOverHttpsMode, "no_match");
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());

  // Test case with policy BuiltInDnsClientEnabled enabled. The DoH fields
  // should be unaffected.
  local_state->Set(prefs::kBuiltInDnsClientEnabled,
                   base::Value(!async_dns_feature_enabled));
  secure_dns_config = GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(!async_dns_feature_enabled, GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.servers(), testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         StubResolverConfigReaderBrowsertest,
                         ::testing::Bool());

}  // namespace
