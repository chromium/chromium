// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_latch.h"

#include <string>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch_factory.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_testing_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

constexpr char kGuestUrl[] = "https://business.gemini.google/side-panel";

// Creates every kind of profile with GEiC enabled and a guest URL configured
// before the profiles are created.
class GlicProviderLatchTest : public testing::Test {
 public:
  GlicProviderLatchTest() {
    feature_list_.InitAndEnableFeature(features::kGeic);
    // Spelled out rather than using `geic::kGeicGuestURLSwitch`: this target
    // can't include geic_enabling.h (see chrome/browser/glic/DEPS).
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        "geic-guest-url", kGuestUrl);
  }

  void SetUp() override { helper_.SetUp(); }

 protected:
  // Expects `profile` to be latched as Gemini Enterprise with the configured
  // guest URL.
  void ExpectGeminiEnterprise(Profile* profile) {
    ASSERT_TRUE(profile);
    EXPECT_EQ(GlicProviderLatch::GetProvider(*profile),
              GlicProvider::kGeminiEnterprise);
    EXPECT_EQ(GetGlicProviderSpec(profile).GetGuestUrl(profile),
              GetLocalizedGuestURL(GURL(kGuestUrl)));
  }

  // Expects off-the-record `profile` to use the same provider and spec as its
  // original profile, which is latched as Gemini Enterprise.
  void ExpectSameAsOriginalProfile(Profile* profile) {
    ASSERT_TRUE(profile);
    ASSERT_TRUE(profile->IsOffTheRecord());
    Profile* original_profile = profile->GetOriginalProfile();
    EXPECT_EQ(GlicProviderLatch::GetProvider(*profile),
              GlicProviderLatch::GetProvider(*original_profile));
    EXPECT_EQ(&GetGlicProviderSpec(profile),
              &GetGlicProviderSpec(original_profile));
    ExpectGeminiEnterprise(profile);
  }

  // Expects `profile` to resolve to the default (fallback) Gemini latch
  // because its original profile isn't latched.
  void ExpectDefaultLatch(Profile* profile) {
    ASSERT_TRUE(profile);
    EXPECT_EQ(GlicProviderLatch::GetProvider(*profile), GlicProvider::kGemini);
    EXPECT_EQ(GetGlicProviderSpec(profile).GetSettingsSectionId(),
              kGeminiSettingsSectionId);
    EXPECT_NE(GetGlicProviderSpec(profile).GetGuestUrl(profile),
              GetLocalizedGuestURL(GURL(kGuestUrl)));
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  ProfileTestingHelper helper_;
};

// Only regular profiles get their own latch.
TEST_F(GlicProviderLatchTest, RegularProfileIsGeminiEnterprise) {
  ExpectGeminiEnterprise(helper_.regular_profile());
}

// Off-the-record profiles belong to the same user as their original profile,
// so they use its provider (e.g. an enterprise user's incognito window is still
// Gemini Enterprise).
TEST_F(GlicProviderLatchTest, IncognitoProfileUsesOriginalProfile) {
  ExpectSameAsOriginalProfile(helper_.incognito_profile());
}

TEST_F(GlicProviderLatchTest, IsolatedModeProfileUsesOriginalProfile) {
  ExpectSameAsOriginalProfile(helper_.isolated_mode_profile());
}

// Only regular profiles get their own latch; off-the-record profiles read their
// original profile's latch.
TEST_F(GlicProviderLatchTest, OffTheRecordProfilesHaveNoLatchOfTheirOwn) {
  KeyedServiceBaseFactory* factory = GlicProviderLatchFactory::GetInstance();
  for (Profile* profile :
       {helper_.incognito_profile(), helper_.isolated_mode_profile()}) {
    factory->CreateServiceNowForTesting(profile);
    EXPECT_FALSE(factory->IsServiceCreated(profile));
  }
}

// Guest, system and Ash internal profiles aren't regular profiles, so neither
// they nor their original profile are latched, and they get the default Gemini
// latch. Glic is disabled for them anyway (GlicEnabling::IsProfileEligible()).
TEST_F(GlicProviderLatchTest, GuestProfilesUseDefaultLatch) {
  ExpectDefaultLatch(helper_.guest_profile());
  ExpectDefaultLatch(helper_.guest_profile_otr());
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(GlicProviderLatchTest, SystemProfilesUseDefaultLatch) {
  ExpectDefaultLatch(helper_.system_profile());
  ExpectDefaultLatch(helper_.system_profile_otr());
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(GlicProviderLatchTest, AshInternalProfilesUseDefaultLatch) {
  ExpectDefaultLatch(helper_.signin_profile());
  ExpectDefaultLatch(helper_.signin_profile_otr());
  ExpectDefaultLatch(helper_.lockscreen_profile());
  ExpectDefaultLatch(helper_.lockscreen_profile_otr());
  ExpectDefaultLatch(helper_.shimless_rma_app_profile());
  ExpectDefaultLatch(helper_.shimless_rma_app_profile_otr());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// The provider is decided once, when the profile is created; later
// configuration changes don't affect it.
TEST_F(GlicProviderLatchTest, LatchDoesNotChangeWithConfiguration) {
  Profile* profile = helper_.regular_profile();
  profile->GetPrefs()->SetString(
      prefs::kGlicGeminiEnterpriseUrl,
      "https://business.gemini.google/home/cid/updated");
  scoped_command_line_.GetProcessCommandLine()->RemoveSwitch("geic-guest-url");

  ExpectGeminiEnterprise(profile);
}

// A latch set for testing before keyed services are created wins over the
// configured provider.
class GlicProviderLatchSetForTestingTest : public GlicProviderLatchTest {
 public:
  void SetUp() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  GlicProviderLatchFactory::SetProviderForTesting(
                      context, GlicProvider::kGemini, /*geic_options=*/{});
                }));
    GlicProviderLatchTest::SetUp();
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

TEST_F(GlicProviderLatchSetForTestingTest, OverridesResolvedLatch) {
  // GEiC is configured, so the factory would latch Gemini Enterprise.
  ExpectDefaultLatch(helper_.regular_profile());
}

// Without GEiC configuration, regular profiles are latched as Gemini.
TEST(GlicProviderLatchDefaultTest, ProfileIsLatchedAsGemini) {
  ProfileTestingHelper helper;
  helper.SetUp();
  Profile* profile = helper.regular_profile();

  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile), GlicProvider::kGemini);
  EXPECT_NE(GetGlicProviderSpec(profile).GetGuestUrl(profile),
            GetLocalizedGuestURL(GURL(kGuestUrl)));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
struct AssistantSelectionLatchTestCase {
  const char* test_name;
  bool has_enterprise_url;
  prefs::AssistantSelection assistant_selection;
  GlicProvider expected_provider;
};

class GlicProviderLatchAssistantSelectionTest
    : public GlicProviderLatchTest,
      public testing::WithParamInterface<AssistantSelectionLatchTestCase> {};

TEST_P(GlicProviderLatchAssistantSelectionTest, ResolvesExpectedProvider) {
  const AssistantSelectionLatchTestCase& param = GetParam();
  scoped_command_line_.GetProcessCommandLine()->RemoveSwitch("geic-guest-url");

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider;
  policy_provider.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);

  auto prefs = std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());
  if (param.has_enterprise_url) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiEnterpriseUrl,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(kGuestUrl), nullptr);
    policy_provider.UpdateChromePolicy(policies);
    prefs->SetManagedPref(prefs::kGlicGeminiEnterpriseUrl,
                          base::Value(kGuestUrl));
  }
  prefs->SetInteger(prefs::kGlicAssistantSelection,
                    static_cast<int>(param.assistant_selection));

  policy::PolicyServiceImpl::Providers providers = {&policy_provider};
  TestingProfile::Builder builder;
  builder.SetPolicyService(
      std::make_unique<policy::PolicyServiceImpl>(std::move(providers)));
  builder.SetPrefService(std::move(prefs));
  std::unique_ptr<TestingProfile> profile = builder.Build();

  if (param.expected_provider == GlicProvider::kGeminiEnterprise) {
    ExpectGeminiEnterprise(profile.get());
  } else {
    ExpectDefaultLatch(profile.get());
  }
  EXPECT_EQ(
      GetGlicProviderSpec(profile.get()).CanSwitchBetweenGeminiProviders(),
      param.has_enterprise_url);

  profile.reset();
  policy_provider.Shutdown();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicProviderLatchAssistantSelectionTest,
    testing::Values(
        // Unset enterprise URL always latches consumer Gemini regardless of
        // assistant selection.
        AssistantSelectionLatchTestCase{"Unset_Auto", false,
                                        prefs::AssistantSelection::kAuto,
                                        GlicProvider::kGemini},
        AssistantSelectionLatchTestCase{"Unset_Consumer", false,
                                        prefs::AssistantSelection::kConsumer,
                                        GlicProvider::kGemini},
        AssistantSelectionLatchTestCase{"Unset_Enterprise", false,
                                        prefs::AssistantSelection::kEnterprise,
                                        GlicProvider::kGemini},
        // Configured enterprise URL latches Gemini Enterprise for kAuto and
        // kEnterprise, and consumer Gemini for kConsumer.
        AssistantSelectionLatchTestCase{"Configured_Auto", true,
                                        prefs::AssistantSelection::kAuto,
                                        GlicProvider::kGeminiEnterprise},
        AssistantSelectionLatchTestCase{"Configured_Consumer", true,
                                        prefs::AssistantSelection::kConsumer,
                                        GlicProvider::kGemini},
        AssistantSelectionLatchTestCase{"Configured_Enterprise", true,
                                        prefs::AssistantSelection::kEnterprise,
                                        GlicProvider::kGeminiEnterprise}),
    [](const testing::TestParamInfo<AssistantSelectionLatchTestCase>& info) {
      return info.param.test_name;
    });
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// The GE guest URL key is the latched guest URL without `hl`.
TEST_F(GlicProviderLatchTest, GeminiEnterpriseGuestUrlKey) {
  Profile* profile = helper_.regular_profile();
  EXPECT_EQ(GetGlicProviderSpec(profile).GetGuestUrlKey(), kGuestUrl);
}

std::string GetGeminiEnterpriseGuestUrlKey(const char* guest_url) {
  return CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise,
                                {.guest_url = GURL(guest_url)})
      ->GetGuestUrlKey();
}

TEST(GlicProviderSpecTest, GeminiEnterpriseGuestUrlKeyDropsLocale) {
  EXPECT_EQ(GetGeminiEnterpriseGuestUrlKey(
                "https://business.gemini.google/side-panel?configId=abc&hl=en"),
            "https://business.gemini.google/side-panel?configId=abc");
  EXPECT_EQ(GetGeminiEnterpriseGuestUrlKey(
                "https://business.gemini.google/side-panel?hl=en&configId=abc"),
            "https://business.gemini.google/side-panel?configId=abc");
}

TEST(GlicProviderSpecTest, GeminiEnterpriseGuestUrlKeyDropsEmptyQuery) {
  EXPECT_EQ(GetGeminiEnterpriseGuestUrlKey(
                "https://business.gemini.google/side-panel?hl=en"),
            "https://business.gemini.google/side-panel");
}

TEST(GlicProviderSpecTest, GeminiEnterpriseGuestUrlKeyKeepsOtherParams) {
  EXPECT_EQ(GetGeminiEnterpriseGuestUrlKey(
                "https://business.gemini.google/side-panel?configId=abc&cid=1"),
            "https://business.gemini.google/side-panel?configId=abc&cid=1");
  EXPECT_EQ(GetGeminiEnterpriseGuestUrlKey(
                "https://business.gemini.google/side-panel"),
            "https://business.gemini.google/side-panel");
}

// GE profiles are only latched with a valid guest URL.
TEST(GlicProviderSpecTest, GeminiEnterpriseWithoutGuestUrlCrashes) {
  EXPECT_CHECK_DEATH(GetGeminiEnterpriseGuestUrlKey(""));
}

// The consumer guest URL key, for profiles and the fallback spec, is the guest
// URL host.
TEST(GlicProviderLatchDefaultTest, GeminiGuestUrlKeyIsHost) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ::switches::kGlicGuestURL,
      "https://autopush.gemini.google.com/glic?foo=bar");
  ProfileTestingHelper helper;
  helper.SetUp();
  EXPECT_EQ(GetGlicProviderSpec(helper.regular_profile()).GetGuestUrlKey(),
            "autopush.gemini.google.com");
  EXPECT_EQ(GetGlicProviderSpec(nullptr).GetGuestUrlKey(),
            "autopush.gemini.google.com");
}

// A Gemini guest URL without a host falls back to the prod Gemini key.
TEST(GlicProviderLatchDefaultTest, GeminiGuestUrlWithoutHostUsesFallbackKey) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ::switches::kGlicGuestURL, "file:///foo");
  EXPECT_EQ(GetGlicProviderSpec(nullptr).GetGuestUrlKey(), kGeminiGuestUrlKey);
}

// Only the consumer web client ever ran in the webview.
TEST(GlicProviderSpecTest, MigratesWebviewLocalStorage) {
  EXPECT_TRUE(CreateGlicProviderSpec(GlicProvider::kGemini, {})
                  ->MigratesWebviewLocalStorage());
  EXPECT_FALSE(CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise,
                                      {.guest_url = GURL(kGuestUrl)})
                   ->MigratesWebviewLocalStorage());
}

}  // namespace
}  // namespace glic
