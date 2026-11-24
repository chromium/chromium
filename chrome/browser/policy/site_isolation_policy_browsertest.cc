// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/site_isolation_policy.h"

#include <memory>

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/advanced_protection_status_manager_test_util.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/ui_test_utils.h"
#endif

class SiteIsolationPolicyBrowserTest : public PlatformBrowserTest {
 public:
  SiteIsolationPolicyBrowserTest(const SiteIsolationPolicyBrowserTest&) =
      delete;
  SiteIsolationPolicyBrowserTest& operator=(
      const SiteIsolationPolicyBrowserTest&) = delete;

 protected:
  SiteIsolationPolicyBrowserTest() = default;

  struct Expectations {
    const char* url;
    bool isolated;
  };

  void CheckExpectations(base::span<Expectations> expectations) {
    content::BrowserContext* context = chrome_test_utils::GetProfile(this);
    for (auto& expectation : expectations) {
      const GURL url(expectation.url);
      auto instance = content::SiteInstance::CreateForURL(context, url);
      EXPECT_EQ(expectation.isolated, instance->RequiresDedicatedProcess())
          << "; url = " << url;
    }
  }

  void CheckIsolatedOriginExpectations(base::span<Expectations> expectations) {
    if (!content::AreAllSitesIsolatedForTesting()) {
      CheckExpectations(expectations);
    }

    auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
    for (auto& expectation : expectations) {
      const GURL url(expectation.url);
      const url::Origin origin = url::Origin::Create(url);
      EXPECT_EQ(expectation.isolated,
                policy->IsGloballyIsolatedOriginForTesting(origin))
          << "; origin = " << origin;
    }
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

class IsolateOriginsShortlistFeaturePolicyBrowserTest
    : public SiteIsolationPolicyBrowserTest {
 public:
  IsolateOriginsShortlistFeaturePolicyBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        site_isolation::features::kIsolateOriginsShortlist);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

template <bool policy_value>
class SitePerProcessPolicyBrowserTest : public SiteIsolationPolicyBrowserTest {
 public:
  SitePerProcessPolicyBrowserTest(const SitePerProcessPolicyBrowserTest&) =
      delete;
  SitePerProcessPolicyBrowserTest& operator=(
      const SitePerProcessPolicyBrowserTest&) = delete;

 protected:
  SitePerProcessPolicyBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    // We setup the policy here, because the policy must be 'live' before
    // the renderer is created, since the value for this policy is passed
    // to the renderer via a command-line. Setting the policy in the test
    // itself or in SetUpOnMainThread works for update-able policies, but
    // is too late for this one.
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;

#if BUILDFLAG(IS_ANDROID)
    const char* kPolicyName = policy::key::kSitePerProcessAndroid;
#else
    const char* kPolicyName = policy::key::kSitePerProcess;
#endif
    values.Set(kPolicyName, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(policy_value), nullptr);
    provider_.UpdateChromePolicy(values);
  }
};

typedef SitePerProcessPolicyBrowserTest<true>
    SitePerProcessPolicyBrowserTestEnabled;
typedef SitePerProcessPolicyBrowserTest<false>
    SitePerProcessPolicyBrowserTestDisabled;

// Ensure that --disable-site-isolation-trials and/or
// --disable-site-isolation-for-enterprise-policy do not override policies.
class NoOverrideSitePerProcessPolicyBrowserTest
    : public SitePerProcessPolicyBrowserTestEnabled {
 public:
  NoOverrideSitePerProcessPolicyBrowserTest(
      const NoOverrideSitePerProcessPolicyBrowserTest&) = delete;
  NoOverrideSitePerProcessPolicyBrowserTest& operator=(
      const NoOverrideSitePerProcessPolicyBrowserTest&) = delete;

 protected:
  NoOverrideSitePerProcessPolicyBrowserTest() = default;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
#if BUILDFLAG(IS_ANDROID)
    command_line->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
#endif
  }
};

IN_PROC_BROWSER_TEST_F(SitePerProcessPolicyBrowserTestEnabled, Simple) {
  Expectations expectations[] = {
      {"https://foo.com/noodles.html", true},
      {"http://foo.com/", true},
      {"http://example.org/pumpkins.html", true},
  };
  CheckExpectations(expectations);
}

class IsolateOriginsPolicyBrowserTest
    : public IsolateOriginsShortlistFeaturePolicyBrowserTest {
 public:
  IsolateOriginsPolicyBrowserTest(const IsolateOriginsPolicyBrowserTest&) =
      delete;
  IsolateOriginsPolicyBrowserTest& operator=(
      const IsolateOriginsPolicyBrowserTest&) = delete;

  IsolateOriginsPolicyBrowserTest()
      : memory_override_(
            std::make_unique<base::test::ScopedAmountOfPhysicalMemoryOverride>(
                base::MiB(4000))) {}
  ~IsolateOriginsPolicyBrowserTest() override = default;

 private:
  std::unique_ptr<base::test::ScopedAmountOfPhysicalMemoryOverride> memory_override_;

 protected:

  void SetUpInProcessBrowserTestFixture() override {
    // We setup the policy here, because the policy must be 'live' before
    // the renderer is created, since the value for this policy is passed
    // to the renderer via a command-line. Setting the policy in the test
    // itself or in SetUpOnMainThread works for update-able policies, but
    // is too late for this one.
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(
        policy::key::kIsolateOrigins, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value("https://policy1.example.org/,http://policy2.example.com"),
        nullptr);
    provider_.UpdateChromePolicy(values);
  }
};

IN_PROC_BROWSER_TEST_F(IsolateOriginsPolicyBrowserTest, Simple) {
  // Verify that the policy present at browser startup is correctly applied.
  Expectations expectations[] = {
      {"https://foo.com/noodles.html", false},
      {"http://foo.com/", false},
      {"https://policy1.example.org/pumpkins.html", true},
      {"http://policy2.example.com/index.php", true},
  };
  CheckIsolatedOriginExpectations(expectations);

  // Simulate updating the policy at "browser runtime".
  policy::PolicyMap values;
  values.Set(
      policy::key::kIsolateOrigins, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value("https://policy3.example.org/,http://policy4.example.com"),
      nullptr);
  provider_.UpdateChromePolicy(values);

  // Verify that the policy update above has taken effect:
  // - policy3 and policy4 origins should become isolated
  // - policy1 and policy2 origins will remain isolated, even though they were
  //   removed from the policy (this is an artifact caused by limitations of
  //   the current implementation, not something that is a hard requirement).
  Expectations expectations2[] = {
      {"https://foo.com/noodles.html", false},
      {"http://foo.com/", false},
      {"https://policy1.example.org/pumpkins.html", true},
      {"http://policy2.example.com/index.php", true},
      {"https://policy3.example.org/pumpkins.html", true},
      {"http://policy4.example.com/index.php", true},
  };
  CheckIsolatedOriginExpectations(expectations2);
}

#if BUILDFLAG(IS_ANDROID)
class IsolateOriginsShortlistPolicyBrowserTest
    : public IsolateOriginsShortlistFeaturePolicyBrowserTest {
 public:
  IsolateOriginsShortlistPolicyBrowserTest(
      const IsolateOriginsShortlistPolicyBrowserTest&) = delete;
  IsolateOriginsShortlistPolicyBrowserTest& operator=(
      const IsolateOriginsShortlistPolicyBrowserTest&) = delete;

  IsolateOriginsShortlistPolicyBrowserTest()
      : memory_override_(
            std::make_unique<base::test::ScopedAmountOfPhysicalMemoryOverride>(
                base::MiB(2000))) {}  // 2GB
  ~IsolateOriginsShortlistPolicyBrowserTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(
        policy::key::kIsolateOriginsShortlist, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value("https://shortlist-policy.example.org/"), nullptr);
    provider_.UpdateChromePolicy(values);
  }

 private:
  std::unique_ptr<base::test::ScopedAmountOfPhysicalMemoryOverride> memory_override_;
};

IN_PROC_BROWSER_TEST_F(IsolateOriginsShortlistPolicyBrowserTest, Simple) {
  Expectations expectations[] = {
      {"https://shortlist-policy.example.org/", true},
      {"https://foo.com/", false},
  };
  CheckIsolatedOriginExpectations(expectations);
}

class IsolateOriginsMultiplePolicyBrowserTest
    : public IsolateOriginsShortlistFeaturePolicyBrowserTest {
 public:
  IsolateOriginsMultiplePolicyBrowserTest() = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(
        policy::key::kIsolateOrigins, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value("https://standard-policy.example.org/"), nullptr);
    values.Set(
        policy::key::kIsolateOriginsShortlist, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value("https://shortlist-policy.example.org/"), nullptr);
    values.Set(
        policy::key::kIsolateOriginsAndroid, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value("https://legacy-policy.example.org/"), nullptr);
    provider_.UpdateChromePolicy(values);
  }
};

class IsolateOriginsMultipleHighEndPolicyBrowserTest
    : public IsolateOriginsMultiplePolicyBrowserTest {
 public:
  IsolateOriginsMultipleHighEndPolicyBrowserTest()
      : memory_override_(
            std::make_unique<base::test::ScopedAmountOfPhysicalMemoryOverride>(
                base::MiB(4000))) {}  // 4GB
  ~IsolateOriginsMultipleHighEndPolicyBrowserTest() override = default;

 private:
  std::unique_ptr<base::test::ScopedAmountOfPhysicalMemoryOverride> memory_override_;
};

IN_PROC_BROWSER_TEST_F(IsolateOriginsMultipleHighEndPolicyBrowserTest,
                       HighEndPriority) {
  // On High-End devices, IsolateOrigins (standard) should take precedence
  // over both shortlist and legacy Android policies.
  Expectations expectations[] = {
      {"https://standard-policy.example.org/", true},
      {"https://shortlist-policy.example.org/", false},
      {"https://legacy-policy.example.org/", false},
  };
  CheckIsolatedOriginExpectations(expectations);
}

class IsolateOriginsMultipleLowEndPolicyBrowserTest
    : public IsolateOriginsMultiplePolicyBrowserTest {
 public:
  IsolateOriginsMultipleLowEndPolicyBrowserTest()
      : memory_override_(
            std::make_unique<base::test::ScopedAmountOfPhysicalMemoryOverride>(
                base::MiB(2000))) {}  // 2GB
  ~IsolateOriginsMultipleLowEndPolicyBrowserTest() override = default;

 private:
  std::unique_ptr<base::test::ScopedAmountOfPhysicalMemoryOverride> memory_override_;
};

IN_PROC_BROWSER_TEST_F(IsolateOriginsMultipleLowEndPolicyBrowserTest,
                       LowEndPriority) {
  // On Low-End devices, IsolateOriginsShortlist should take precedence
  // over the deprecated IsolateOriginsAndroid policy, and IsolateOrigins (standard)
  // should be ignored.
  Expectations expectations[] = {
      {"https://standard-policy.example.org/", false},
      {"https://shortlist-policy.example.org/", true},
      {"https://legacy-policy.example.org/", false},
  };
  CheckIsolatedOriginExpectations(expectations);
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(NoOverrideSitePerProcessPolicyBrowserTest, Simple) {
  Expectations expectations[] = {
      {"https://foo.com/noodles.html", true},
      {"http://example.org/pumpkins.html", true},
  };
  CheckExpectations(expectations);
}

// After https://crbug.com/41429044 was fixed, enterprise policy can only be
// used to disable Site Isolation on Android - the
// SitePerProcessPolicyBrowserTestFieldTrialTest tests should not be run on any
// other platform.  Note that browser_tests won't run on Android until
// https://crbug.com/40469222 is fixed.
#if BUILDFLAG(IS_ANDROID)
class SitePerProcessPolicyBrowserTestFieldTrialTest
    : public SitePerProcessPolicyBrowserTestDisabled {
 public:
  SitePerProcessPolicyBrowserTestFieldTrialTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kSitePerProcess);
  }
  SitePerProcessPolicyBrowserTestFieldTrialTest(
      const SitePerProcessPolicyBrowserTestFieldTrialTest&) = delete;
  SitePerProcessPolicyBrowserTestFieldTrialTest& operator=(
      const SitePerProcessPolicyBrowserTestFieldTrialTest&) = delete;
  ~SitePerProcessPolicyBrowserTestFieldTrialTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SitePerProcessPolicyBrowserTestFieldTrialTest, Simple) {
  // Skip this test if the --site-per-process switch is present (e.g. on Site
  // Isolation Android chromium.fyi bot).  The test is still valid if
  // SitePerProcess is the default (e.g. via ContentBrowserClient's
  // ShouldEnableStrictSiteIsolation method) - don't skip the test in such case.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSitePerProcess)) {
    return;
  }

  // Policy should inject kDisableSiteIsolationForPolicy rather than
  // kDisableSiteIsolation switch.
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolation));
  ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolationForPolicy));
  EXPECT_FALSE(
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());

  Expectations expectations[] = {
      {"https://foo.com/noodles.html", false},
      {"http://example.org/pumpkins.html", false},
  };
  CheckExpectations(expectations);
}
#endif

// Checks that the SitePerProcess (SitePerProcessAndroid on Android) enterprise
// policy takes precedence over features::kExcludeAdsFromSiteIsolation, so that
// all sites are isolated, including ads.
class SitePerProcessPolicyAdsExclusionBrowserTest
    : public SitePerProcessPolicyBrowserTestEnabled {
 public:
  SitePerProcessPolicyAdsExclusionBrowserTest() {
    feature_list_.InitWithFeatures({features::kExcludeAdsFromSiteIsolation,
                                    features::kDefaultSiteInstanceGroups},
                                   {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SitePerProcessPolicyAdsExclusionBrowserTest,
                       FullSiteIsolationWins) {
#if BUILDFLAG(IS_ANDROID)
  // Simulate a device with enough memory for the ads exclusion mode, so that
  // only the policy can turn the mode off.
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::MiB(8000));
  site_isolation::SiteIsolationPolicy::
      SetDisallowMemoryThresholdCachingForTesting(true);
#endif  // BUILDFLAG(IS_ANDROID)

  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
  EXPECT_FALSE(content::SiteIsolationPolicy::
                   UseDedicatedProcessesForAllSitesExceptAds());
  EXPECT_EQ(content::SiteIsolationDisabledReason::kNotDisabled,
            content::SiteIsolationPolicy::GetSiteIsolationDisabledReason());
}

#if BUILDFLAG(IS_ANDROID)
namespace {
bool CheckUseDedicatedProcessesForAllSitesWithAndroidState(
    bool is_under_advanced_protection,
    base::ByteSize ram) {
  safe_browsing::SetAdvancedProtectionStateForTesting(
      is_under_advanced_protection);
  ChromeContentBrowserClient::DisableAdvancedProtectionCachingForTests();

  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(ram);
  site_isolation::SiteIsolationPolicy::
      SetDisallowMemoryThresholdCachingForTesting(true);
  return content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
}
}  // anonymous namespace
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(SiteIsolationPolicyBrowserTest,
                       NoPolicyNoTrialsFlags_NoAdvancedProtection_HighRam) {
  // The switch to disable Site Isolation should be missing by default (i.e.
  // without an explicit enterprise policy).
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolation));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolationForPolicy));
  EXPECT_EQ(CheckUseDedicatedProcessesForAllSitesWithAndroidState(
                /*is_under_advanced_protection=*/false,
                // TODO(crbug.com/429140103): Comments in the original code
                // suggested that this was in KiB, but it was in fact in MiB.
                // Needs investigation.
                /*ram=*/base::MiB(8000)),
            base::FeatureList::IsEnabled(features::kSitePerProcess));
#else
  EXPECT_TRUE(content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SiteIsolationPolicyBrowserTest,
                       NoPolicy_AdvancedProtection_HighRam) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  EXPECT_FALSE(command_line->HasSwitch(switches::kDisableSiteIsolation));
  EXPECT_FALSE(
      command_line->HasSwitch(switches::kDisableSiteIsolationForPolicy));
  EXPECT_TRUE(CheckUseDedicatedProcessesForAllSitesWithAndroidState(
      /*is_under_advanced_protection=*/true,
      // TODO(crbug.com/429140103): Comments in the original code suggested that
      // this was in KiB, but it was in fact in MiB. Needs investigation.
      /*ram=*/base::MiB(8000)));
}

IN_PROC_BROWSER_TEST_F(SiteIsolationPolicyBrowserTest,
                       NoPolicy_AdvancedProtection_LowRam) {
  // This test relies on site isolation memory thresholds being enabled. Skip
  // if that feature is disable.
  if (!base::FeatureList::IsEnabled(
          site_isolation::features::
              kSiteIsolationEnableMemoryThresholdAndroid)) {
    GTEST_SKIP();
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Skip this test if the --site-per-process switch is present (e.g. on Site
  // Isolation Android chromium.fyi bot).
  if (command_line->HasSwitch(switches::kSitePerProcess)) {
    return;
  }

  EXPECT_FALSE(command_line->HasSwitch(switches::kDisableSiteIsolation));
  EXPECT_FALSE(
      command_line->HasSwitch(switches::kDisableSiteIsolationForPolicy));
  EXPECT_FALSE(CheckUseDedicatedProcessesForAllSitesWithAndroidState(
      /*is_under_advanced_protection=*/true,
      // TODO(crbug.com/429140103): Comments in the original code suggested that
      // this was in KiB, but it was in fact in MiB. Needs investigation.
      /*ram=*/base::MiB(1000)));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
// Parameterized test class to check that an enterprise policy can set the
// origin-keyed processes by default feature, but a user can override the value
// that was set by the enterprise policy (via either the command-line flags or
// about:flags).
class OriginKeyedProcessesEnabledPolicyBrowserTest
    : public SiteIsolationPolicyBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  OriginKeyedProcessesEnabledPolicyBrowserTest(
      const OriginKeyedProcessesEnabledPolicyBrowserTest&) = delete;
  OriginKeyedProcessesEnabledPolicyBrowserTest& operator=(
      const OriginKeyedProcessesEnabledPolicyBrowserTest&) = delete;

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  OriginKeyedProcessesEnabledPolicyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (GetParam()) {
      // Turn off the origin-keyed processes feature via user intervention. This
      // will override the enterprise setting which will have it set to be
      // enabled.
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {/*enabled_features*/},
          {/*disabled_features=*/features::kOriginKeyedProcessesByDefault});
    } else {
      // Without user override, set the memory threshold to a high value that
      // would not be reached, normally causing the feature to remain disabled.
      // This will check that overrides will ignore memory threshold limits.
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{site_isolation::features::kOriginIsolationMemoryThreshold,
            {{site_isolation::features::
                  kOriginIsolationMemoryThresholdParamName,
              std::string("524288")}}}},
          {/*disabled_features*/});
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    // We setup the policy here, because the policy must be 'live' before
    // the renderer is created, since the value for this policy is passed
    // to the renderer via a command-line. Setting the policy in the test
    // itself or in SetUpOnMainThread works for update-able policies, but
    // is too late for this one.
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    policy::PolicyMap values;
    values.Set(policy::key::kOriginKeyedProcessesEnabled,
               policy::POLICY_LEVEL_RECOMMENDED, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    provider_.UpdateChromePolicy(values);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());
    net::test_server::RegisterDefaultHandlers(https_server());
    ASSERT_TRUE(https_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OriginKeyedProcessesEnabledPolicyBrowserTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "WithUserOverride"
                                             : "WithoutUserOverride";
                         });

IN_PROC_BROWSER_TEST_P(OriginKeyedProcessesEnabledPolicyBrowserTest, Simple) {
  GURL start_url(https_server()->GetURL("a.test", "/iframe.html"));
  GURL cross_origin_url(
      https_server()->GetURL("crossorigin.a.test", "/simple.html"));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", cross_origin_url));

  content::RenderFrameHost* root_rfh = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_rfh = ChildFrameAt(root_rfh, 0);

  if (GetParam()) {
    // The user overrode the enterprise policy to turn the feature off.
    EXPECT_EQ(root_rfh->GetProcess(), child_rfh->GetProcess())
        << "The root frame and child iframe should be in the same process.";
  } else {
    // There is no user override. The enterprise policy has highest priority and
    // turns the feature on, even if the memory threshold is higher than this
    // machine's physical memory.
    EXPECT_NE(root_rfh->GetProcess(), child_rfh->GetProcess())
        << "The root frame and child iframe should be in separate processes.";
  }
}

namespace {

// Tags every navigation as an ad by the ad host filter.
class AdTaggingThrottle : public content::NavigationThrottle {
 public:
  explicit AdTaggingThrottle(content::NavigationThrottleRegistry& registry)
      : content::NavigationThrottle(registry) {}

  ThrottleCheckResult WillStartRequest() override {
    navigation_handle()->SetIsAdTaggedByHostFilter();
    return PROCEED;
  }

  const char* GetNameForLogging() override { return "AdTaggingThrottle"; }
};

enum class AdsExclusionPolicyTestCase {
  // Ads are not excluded.
  kPolicyWithFieldTrial,
  // Ads are excluded.
  kPolicyWithCommandLine,
  // Control. Ads are excluded.
  kNoPolicyWithFieldTrial,
  // Origin-keyed processes enabled from the command line, which also counts as
  // an override. Ads are not excluded.
  kCommandLineWithFieldTrial,
};

}  // namespace

// Checks that the OriginKeyedProcessesEnabled enterprise policy beats a field
// trial for the ads exclusion.
class OriginKeyedProcessesEnabledPolicyAdsExclusionBrowserTest
    : public SiteIsolationPolicyBrowserTest,
      public testing::WithParamInterface<AdsExclusionPolicyTestCase> {
 public:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  OriginKeyedProcessesEnabledPolicyAdsExclusionBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // ScopedFeatureList initializes FeatureList via InitFromCommandLine().
    // InitAndEnableFeature() does not attach a field trial, so it counts as a
    // command-line override (IsFeatureOverriddenFromCommandLine() is true),
    // whereas ...WithParameters() calls attach a synthetic field trial.
    const base::FieldTrialParams kFieldTrialParams = {{"param", "value"}};
    switch (GetParam()) {
      case AdsExclusionPolicyTestCase::kPolicyWithFieldTrial:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            features::kExcludeAdsFromOriginIsolation, kFieldTrialParams);
        break;
      case AdsExclusionPolicyTestCase::kPolicyWithCommandLine:
        scoped_feature_list_.InitAndEnableFeature(
            features::kExcludeAdsFromOriginIsolation);
        break;
      case AdsExclusionPolicyTestCase::kNoPolicyWithFieldTrial:
        scoped_feature_list_.InitWithFeaturesAndParameters(
            {{features::kOriginKeyedProcessesByDefault, kFieldTrialParams},
             {features::kExcludeAdsFromOriginIsolation, kFieldTrialParams}},
            {});
        break;
      case AdsExclusionPolicyTestCase::kCommandLineWithFieldTrial:
        // Separate lists, since every feature in a ...WithParameters() call
        // gets a field trial.
        okp_command_line_feature_list_.InitAndEnableFeature(
            features::kOriginKeyedProcessesByDefault);
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            features::kExcludeAdsFromOriginIsolation, kFieldTrialParams);
        break;
    }
  }

  bool IsPolicySet() const {
    return GetParam() == AdsExclusionPolicyTestCase::kPolicyWithFieldTrial ||
           GetParam() == AdsExclusionPolicyTestCase::kPolicyWithCommandLine;
  }

  bool ShouldExcludeAds() const {
    return GetParam() == AdsExclusionPolicyTestCase::kPolicyWithCommandLine ||
           GetParam() == AdsExclusionPolicyTestCase::kNoPolicyWithFieldTrial;
  }

  void SetUpInProcessBrowserTestFixture() override {
    if (!IsPolicySet()) {
      return;
    }
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    // This policy is recommended-only.
    policy::PolicyMap values;
    values.Set(policy::key::kOriginKeyedProcessesEnabled,
               policy::POLICY_LEVEL_RECOMMENDED, policy::POLICY_SCOPE_MACHINE,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    provider_.UpdateChromePolicy(values);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    ASSERT_TRUE(https_server()->Start());
  }

 private:
  base::test::ScopedFeatureList okp_command_line_feature_list_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OriginKeyedProcessesEnabledPolicyAdsExclusionBrowserTest,
    testing::Values(AdsExclusionPolicyTestCase::kPolicyWithFieldTrial,
                    AdsExclusionPolicyTestCase::kPolicyWithCommandLine,
                    AdsExclusionPolicyTestCase::kNoPolicyWithFieldTrial,
                    AdsExclusionPolicyTestCase::kCommandLineWithFieldTrial),
    [](const testing::TestParamInfo<AdsExclusionPolicyTestCase>& info) {
      switch (info.param) {
        case AdsExclusionPolicyTestCase::kPolicyWithFieldTrial:
          return "PolicyWithFieldTrial";
        case AdsExclusionPolicyTestCase::kPolicyWithCommandLine:
          return "PolicyWithCommandLine";
        case AdsExclusionPolicyTestCase::kNoPolicyWithFieldTrial:
          return "NoPolicyWithFieldTrial";
        case AdsExclusionPolicyTestCase::kCommandLineWithFieldTrial:
          return "CommandLineWithFieldTrial";
      }
    });

IN_PROC_BROWSER_TEST_P(OriginKeyedProcessesEnabledPolicyAdsExclusionBrowserTest,
                       AdFrame) {
  content::BrowserContext* browser_context =
      chrome_test_utils::GetProfile(this);
  // Avoid IsEnabled(), which activates the field trial.
  base::FeatureList* feature_list = base::FeatureList::GetInstance();
  ASSERT_TRUE(feature_list->IsFeatureOverridden(
      features::kExcludeAdsFromOriginIsolation.name));
  ASSERT_EQ(GetParam() == AdsExclusionPolicyTestCase::kPolicyWithCommandLine,
            feature_list->IsFeatureOverriddenFromCommandLine(
                features::kExcludeAdsFromOriginIsolation.name));
  const PrefService::Preference* policy_pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kOriginKeyedProcessesEnabled);
  ASSERT_TRUE(policy_pref);
  ASSERT_EQ(IsPolicySet(), policy_pref->IsRecommended());
  ASSERT_TRUE(
      content::SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault(
          browser_context));
  EXPECT_EQ(
      ShouldExcludeAds(),
      content::SiteIsolationPolicy::AreAdsExcludedFromOriginKeyedProcesses(
          browser_context));

  GURL start_url(https_server()->GetURL("a.test", "/iframe.html"));
  GURL ad_url(https_server()->GetURL("ad.a.test", "/simple.html"));
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(content::NavigateToURL(web_contents, start_url));

  // Tag the subframe navigation as an ad.
  content::TestNavigationThrottleInserter throttle_inserter(
      web_contents,
      base::BindRepeating([](content::NavigationThrottleRegistry& registry) {
        if (!registry.GetNavigationHandle().IsInMainFrame()) {
          registry.AddThrottle(std::make_unique<AdTaggingThrottle>(registry));
        }
      }));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", ad_url));

  content::RenderFrameHost* child_rfh =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);
  EXPECT_TRUE(
      content::HasOriginKeyedProcess(web_contents->GetPrimaryMainFrame()));
  EXPECT_EQ(!ShouldExcludeAds(), content::HasOriginKeyedProcess(child_rfh));
}
#endif  // !BUILDFLAG(IS_ANDROID)
