// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/stub_resolver_config_reader.h"

#include <string>

#include "base/enterprise_util.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "net/dns/public/secure_dns_mode.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/net/secure_dns_manager.h"
#endif

// TODO(ericorth@chromium.org): Consider validating that the expected
// configuration makes it all the way to the net::HostResolverManager in the
// network service, rather than just testing StubResolverConfigReader output.

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const std::string kDnsOverHttpsTemplatesPrefName =
    prefs::kDnsOverHttpsEffectiveTemplatesChromeOS;
#else
const std::string kDnsOverHttpsTemplatesPrefName =
    prefs::kDnsOverHttpsTemplates;
#endif

class StubResolverConfigReaderBrowsertest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  StubResolverConfigReaderBrowsertest() {
    scoped_feature_list_.InitWithFeatureState(net::features::kAsyncDns,
                                              GetParam());
  }
  ~StubResolverConfigReaderBrowsertest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Normal boilerplate to setup a MockConfigurationPolicyProvider.
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    // Set the mocked policy provider to act as if no policies are in use by
    // updating to the initial still-empty `policy_map_`.
    ASSERT_TRUE(policy_map_.empty());
    policy_provider_.UpdateChromePolicy(policy_map_);

    config_reader_ = SystemNetworkContextManager::GetStubResolverConfigReader();

    // Only affects future parental controls checks, but that is all that
    // matters here because these tests only deal with checking fresh config
    // reads, not what has been sent to Network Service.
    //
    // TODO(ericorth@chromium.org): If validation of network service state is
    // ever added, need to ensure the result of this override gets sent first.
    config_reader_->OverrideParentalControlsForTesting(
        /*parental_controls_override=*/false);
  }

 protected:
  void SetSecureDnsModePolicy(std::string mode_str) {
    policy_map_.Set(
        policy::key::kDnsOverHttpsMode, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
        base::Value(std::move(mode_str)),
        /*external_data_fetcher=*/nullptr);
    policy_provider_.UpdateChromePolicy(policy_map_);
  }

  void SetDohTemplatesPolicy(std::string teplates_str) {
    policy_map_.Set(
        policy::key::kDnsOverHttpsTemplates, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
        base::Value(std::move(teplates_str)),
        /*external_data_fetcher=*/nullptr);
    policy_provider_.UpdateChromePolicy(policy_map_);
  }

  policy::PolicyMap policy_map_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  raw_ptr<StubResolverConfigReader, DanglingUntriaged> config_reader_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Set various DoH modes and DoH template strings and make sure the settings are
// respected.
IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest, ConfigFromPrefs) {
  bool async_dns_feature_enabled = GetParam();

  // Mark as not enterprise managed.
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  // TODO(crbug.com/40229843): What is the correct function to use here?
  EXPECT_FALSE(base::win::IsEnrolledToDomain());
#endif

  std::string good_post_template = "https://foo.test/";
  std::string good_get_template = "https://bar.test/dns-query{?dns}";
  std::string bad_template = "dns-query{?dns}";
  std::string good_then_bad_template = good_get_template + " " + bad_template;
  std::string bad_then_good_template = bad_template + " " + good_get_template;
  std::string multiple_good_templates =
      "  " + good_get_template + "   " + good_post_template + "  ";

  PrefService* pref_service_for_user_settings =
      g_browser_process->local_state();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, the local_state is shared between all users so the user-set
  // pref is stored in the profile's pref service.
  pref_service_for_user_settings = browser()->profile()->GetPrefs();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            SecureDnsConfig::kModeSecure);
  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            bad_template);
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kSecure, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            good_post_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kSecure, secure_dns_config.mode());
  EXPECT_EQ(*net::DnsOverHttpsConfig::FromString(good_post_template),
            secure_dns_config.doh_servers());

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            SecureDnsConfig::kModeAutomatic);
  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            bad_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            good_then_bad_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_EQ(*net::DnsOverHttpsConfig::FromString(good_get_template),
            secure_dns_config.doh_servers());

  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            bad_then_good_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_EQ(*net::DnsOverHttpsConfig::FromString(good_get_template),
            secure_dns_config.doh_servers());

  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            multiple_good_templates);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_EQ(*net::DnsOverHttpsConfig::FromString(multiple_good_templates),
            secure_dns_config.doh_servers());

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            SecureDnsConfig::kModeOff);
  pref_service_for_user_settings->SetString(kDnsOverHttpsTemplatesPrefName,
                                            good_get_template);
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  pref_service_for_user_settings->SetString(prefs::kDnsOverHttpsMode,
                                            "no_match");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  // Test case with policy BuiltInDnsClientEnabled enabled. The DoH fields
  // should be unaffected.
  // The `prefs::kBuiltInDnsClientEnabled` pref is stored on local_state on all
  // platforms (including Chrome OS).
  g_browser_process->local_state()->Set(
      prefs::kBuiltInDnsClientEnabled, base::Value(!async_dns_feature_enabled));
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);
  EXPECT_EQ(!async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest,
                       DefaultNonSetPolicies) {
  bool async_dns_feature_enabled = GetParam();
// Mark as not enterprise managed.
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsEnterpriseDevice());
#endif

  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(async_dns_feature_enabled,
            config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kAutomatic);
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
}

// ChromeOS includes its own special functionality to set default policies if
// any policies are set.  This function is not declared and cannot be invoked
// in non-CrOS builds. Expect these enterprise user defaults to disable DoH.
#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest, SpecialPolicies) {
  // Applies the special ChromeOS defaults to `policy_map_`.
  policy::SetEnterpriseUsersDefaults(&policy_map_);
  // Send the PolicyMap to the mock policy provider.
  policy_provider_.UpdateChromePolicy(policy_map_);
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest,
                       DisableDohByPolicy) {
// Mark as not enterprise managed.
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsEnterpriseDevice());
#endif

  SetSecureDnsModePolicy("off");
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest,
                       AutomaticModeByPolicy) {
// Mark as not enterprise managed.
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsEnterpriseDevice());
#endif

  SetSecureDnsModePolicy("automatic");
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kAutomatic);
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest,
                       SecureModeByPolicy) {
// Mark as not enterprise managed.
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsEnterpriseDevice());
#endif

  SetSecureDnsModePolicy("secure");
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  g_browser_process->local_state()->SetString(
      prefs::kDnsOverHttpsEffectiveTemplatesChromeOS, "https://doh.test/");
#else
  SetDohTemplatesPolicy("https://doh.test/");
#endif
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kSecure);
  EXPECT_EQ(*net::DnsOverHttpsConfig::FromString("https://doh.test/"),
            secure_dns_config.doh_servers());
}

IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest,
                       InvalidTemplatePolicy) {
// Mark as not enterprise managed.
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsEnterpriseDevice());
#endif

  SetSecureDnsModePolicy("secure");
  SetDohTemplatesPolicy("invalid template");
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kSecure);
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
  // Deterministic regression test for flaky failures seen in
  // https://crbug.com/1326526. This induces a DNS resolution while in secure
  // mode with zero DoH server templates to use.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("foo.example", "/")));
}

IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest, InvalidModePolicy) {
// Mark as not enterprise managed.
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsEnterpriseDevice());
#endif

  SetSecureDnsModePolicy("invalid");
  SetDohTemplatesPolicy("https://doh.test/");
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/false);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  // Expect empty templates if mode policy is invalid.
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
}

// Test that parental controls detection interacts correctly with prefs and
// policies.
IN_PROC_BROWSER_TEST_P(StubResolverConfigReaderBrowsertest,
                       ConfigFromParentalControls) {
// Mark as not enterprise managed.
#if BUILDFLAG(IS_WIN)
  base::win::ScopedDomainStateForTesting scoped_domain(false);
  EXPECT_FALSE(base::IsEnterpriseDevice());
#endif

  config_reader_->OverrideParentalControlsForTesting(
      /*parental_controls_override=*/true);

  // Parental controls takes precedence over regular prefs.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/true);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kOff);
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  // Policy takes precedence over parental controls.
  SetSecureDnsModePolicy("automatic");
  SetDohTemplatesPolicy("");
  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      /*force_check_parental_controls_for_automatic_mode=*/true);
  EXPECT_EQ(secure_dns_config.mode(), net::SecureDnsMode::kAutomatic);
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         StubResolverConfigReaderBrowsertest,
                         ::testing::Bool());

}  // namespace
