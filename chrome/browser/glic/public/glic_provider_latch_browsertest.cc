// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_latch.h"

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

// Spelled out rather than using `geic::kGeicGuestURLSwitch`: including
// geic_enabling.h is restricted by chrome/browser/glic/DEPS.
constexpr char kGeicGuestUrlSwitch[] = "geic-guest-url";
constexpr char kGuestUrl[] = "https://business.gemini.google/side-panel";
constexpr char kPolicyUrl[] = "https://business.gemini.google/home/cid/abc123";
// `kPolicyUrl` rewritten to the side-panel endpoint.
constexpr char kCanonicalPolicyUrl[] =
    "https://business.gemini.google/side-panel?configId=abc123";

std::vector<base::test::FeatureRef> GetGeicEnabledFeatures() {
  std::vector<base::test::FeatureRef> features =
      GetDefaultEnabledGlicTestFeatures();
  features.push_back(features::kGeic);
  return features;
}

std::vector<base::test::FeatureRef> GetGeicDisabledFeatures() {
  std::vector<base::test::FeatureRef> features =
      GetDefaultDisabledGlicTestFeatures();
  features.push_back(features::kGeic);
  return features;
}

// Verifies that the provider is latched through the real startup path, i.e.
// when keyed services are created for profiles loaded by the browser, rather
// than through test-only overrides. GEiC is enabled and its guest URL comes
// from the command line.
class GlicProviderLatchBrowserTest : public InProcessBrowserTest {
 public:
  GlicProviderLatchBrowserTest()
      : glic_test_env_(GlicTestEnvironmentConfig{}, GetGeicEnabledFeatures()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(kGeicGuestUrlSwitch, kGuestUrl);
  }

 protected:
  GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(GlicProviderLatchBrowserTest,
                       LatchesGeminiEnterpriseOnStartup) {
  Profile* profile = browser()->GetProfile();
  EXPECT_EQ(GlicProvider::kGeminiEnterprise,
            GlicProviderLatch::GetProvider(*profile));
  EXPECT_EQ(GetLocalizedGuestURL(GURL(kGuestUrl)),
            GetGlicProviderSpec(profile).GetGuestUrl(profile));
}

// ChromeOS does not support multiple profiles.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GlicProviderLatchBrowserTest,
                       LatchesGeminiEnterpriseForNewProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile& second_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);

  EXPECT_EQ(GlicProvider::kGeminiEnterprise,
            GlicProviderLatch::GetProvider(second_profile));
  EXPECT_EQ(GetLocalizedGuestURL(GURL(kGuestUrl)),
            GetGlicProviderSpec(&second_profile).GetGuestUrl(&second_profile));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// With `features::kGeic` disabled, profiles are latched as Gemini even though a
// GEiC guest URL is configured.
class GlicProviderLatchGeicDisabledBrowserTest : public InProcessBrowserTest {
 public:
  GlicProviderLatchGeicDisabledBrowserTest()
      : glic_test_env_(GlicTestEnvironmentConfig{},
                       GetDefaultEnabledGlicTestFeatures(),
                       GetGeicDisabledFeatures()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(kGeicGuestUrlSwitch, kGuestUrl);
  }

 protected:
  GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(GlicProviderLatchGeicDisabledBrowserTest,
                       LatchesGeminiOnStartup) {
  Profile* profile = browser()->GetProfile();
  EXPECT_EQ(GlicProvider::kGemini, GlicProviderLatch::GetProvider(*profile));
  EXPECT_NE(GetLocalizedGuestURL(GURL(kGuestUrl)),
            GetGlicProviderSpec(profile).GetGuestUrl(profile));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GlicProviderLatchGeicDisabledBrowserTest,
                       LatchesGeminiForNewProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile& second_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);

  EXPECT_EQ(GlicProvider::kGemini,
            GlicProviderLatch::GetProvider(second_profile));
  EXPECT_NE(GetLocalizedGuestURL(GURL(kGuestUrl)),
            GetGlicProviderSpec(&second_profile).GetGuestUrl(&second_profile));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// With `features::kGeic` enabled and the GEiC guest URL set by the
// GeminiEnterpriseUrl policy (and no command-line switch), profiles are
// latched as Gemini Enterprise. The policy is set before the browser starts, so
// it is loaded before the profile's keyed services are created.
class GlicProviderLatchGeicPolicyBrowserTest : public InProcessBrowserTest {
 public:
  GlicProviderLatchGeicPolicyBrowserTest()
      : glic_test_env_(GlicTestEnvironmentConfig{}, GetGeicEnabledFeatures()) {}

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiEnterpriseUrl,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(kPolicyUrl), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(GlicProviderLatchGeicPolicyBrowserTest,
                       LatchesGeminiEnterpriseOnStartup) {
  Profile* profile = browser()->GetProfile();
  EXPECT_EQ(GlicProvider::kGeminiEnterprise,
            GlicProviderLatch::GetProvider(*profile));
  EXPECT_EQ(GetLocalizedGuestURL(GURL(kCanonicalPolicyUrl)),
            GetGlicProviderSpec(profile).GetGuestUrl(profile));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GlicProviderLatchGeicPolicyBrowserTest,
                       LatchesGeminiEnterpriseForNewProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile& second_profile =
      profiles::testing::CreateProfileSync(profile_manager, new_path);

  EXPECT_EQ(GlicProvider::kGeminiEnterprise,
            GlicProviderLatch::GetProvider(second_profile));
  EXPECT_EQ(GetLocalizedGuestURL(GURL(kCanonicalPolicyUrl)),
            GetGlicProviderSpec(&second_profile).GetGuestUrl(&second_profile));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace glic
