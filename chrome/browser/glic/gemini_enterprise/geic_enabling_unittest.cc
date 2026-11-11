// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/google_groups_manager.h"
#include "components/variations/service/google_groups_manager_prefs.h"
#include "components/variations/variations_seed_processor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

class GeicEnablingTest : public testing::Test {
 public:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
  }

  void TearDown() override {
    profile_.reset();
    policy_provider_.Shutdown();
  }

 protected:
  TestingProfile* profile() {
    if (!profile_) {
      policy::PolicyServiceImpl::Providers providers = {&policy_provider_};
      TestingProfile::Builder builder;
      builder.SetPolicyService(
          std::make_unique<policy::PolicyServiceImpl>(std::move(providers)));
      profile_ = builder.Build();
    }
    return profile_.get();
  }

  void SetPolicyUrl(const std::string& url) {
    profile()->GetPrefs()->SetString(glic::prefs::kGlicGeminiEnterpriseUrl,
                                     url);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  void SetPolicyWithScope(const std::string& url, policy::PolicyScope scope) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiEnterpriseUrl,
                 policy::POLICY_LEVEL_MANDATORY, scope,
                 policy::POLICY_SOURCE_CLOUD, base::Value(url), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
    SetPolicyUrl(url);
  }
#endif

  void SetSwitchUrl(const std::string& url) {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        kGeicGuestURLSwitch, url);
  }

  GURL ResolvePolicyUrl(const std::string& url) {
    SetPolicyUrl(url);
    return ResolveGeicGuestUrl(profile());
  }

  // Sets the command-line switch URL and resolves the guest URL.
  GURL ResolveSwitchUrl(const std::string& url) {
    SetSwitchUrl(url);
    return ResolveGeicGuestUrl(profile());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedCommandLine scoped_command_line_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(GeicEnablingTest, DefaultState) {
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(IsGeicEnabledByFeature());
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
  EXPECT_TRUE(
      ResolvePolicyUrl("https://business.gemini.google/side-panel").is_empty());
#else
  EXPECT_TRUE(IsGeicEnabledByFeature());
  // Without a configured guest URL, a profile is not in GEiC mode even though
  // `features::kGeic` is enabled by default.
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
  EXPECT_EQ(ResolvePolicyUrl("https://business.gemini.google/side-panel"),
            GURL("https://business.gemini.google/side-panel"));
#endif
}

TEST_F(GeicEnablingTest, FinchEnabledWithDefaults) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_TRUE(IsGeicEnabledByFeature());
  // Without a resolved guest URL, a profile is not in GEiC mode.
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
}

TEST_F(GeicEnablingTest, FinchEnabledWithExplicitDisabledParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "false"},
         {"geic-guest-url", "https://business.gemini.google/side-panel"}}}},
      {});

  EXPECT_FALSE(IsGeicEnabledByFeature());
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
}

TEST_F(GeicEnablingTest, FinchEnabledWithCustomGuestUrlParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url", "https://business.gemini.google/side-panel"}}}},
      {});

  EXPECT_TRUE(IsGeicEnabledByFeature());
  EXPECT_EQ(ResolveGeicGuestUrl(profile()),
            GURL("https://business.gemini.google/side-panel"));
}

TEST_F(GeicEnablingTest, FlagExplicitlyDisabledOverridesFinch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {},
      {{features::kGeic}});  // Explicitly disabled (e.g. via chrome://flags)
  SetSwitchUrl("https://business.gemini.google/cli-panel");

  EXPECT_FALSE(IsGeicEnabledByFeature());
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
}

TEST_F(GeicEnablingTest, FlagExplicitlyEnabledBypassesChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_TRUE(IsGeicEnabledByFeature());
}

TEST_F(GeicEnablingTest, CommandLineSwitchOverridesGuestUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);
  SetSwitchUrl("https://business.gemini.google/cli-panel");

  EXPECT_EQ(ResolveGeicGuestUrl(profile()),
            GURL("https://business.gemini.google/cli-panel"));
}

TEST_F(GeicEnablingTest, AcceptsAllowedHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_FALSE(ResolvePolicyUrl("https://business.gemini.google/").is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://enterprise.gemini.google.com/").is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://enterprise.gemini.cloud.google/").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("https://enterprise.gemini.cloud.google.com/")
                   .is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("https://gemini.cloud.google/").is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://vertexaisearch.cloud.google/").is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://vertexaisearch.cloud.google.com/").is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://vertexaisearch.gemini-us.google/").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("https://ucs-widget.corp.google.com/side-panel")
                   .is_empty());
}

TEST_F(GeicEnablingTest, PolicyRejectsPreProductionHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_TRUE(
      ResolvePolicyUrl("https://ucs-app-autopush.corp.google.com/side-panel")
          .is_empty());
  EXPECT_TRUE(
      ResolvePolicyUrl("https://business-staging.gemini.google/side-panel")
          .is_empty());
}

TEST_F(GeicEnablingTest, FinchAcceptsPreProductionHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGeic,
      {{"geic-guest-url",
        "https://ucs-app-autopush.corp.google.com/side-panel"}});

  EXPECT_EQ(ResolveGeicGuestUrl(profile()),
            GURL("https://ucs-app-autopush.corp.google.com/side-panel"));
}

TEST_F(GeicEnablingTest, FinchRejectsDisallowedPreProductionUrls) {
  const char* const kDisallowed[] = {
      // Not matching a product-prefixed pattern.
      "https://foo.corp.google.com/side-panel",
      "https://a.business-staging.gemini.google/side-panel",
      // Not HTTPS.
      "http://business-staging.gemini.google/side-panel",
      // Embedded credentials.
      "https://user:pass@business-staging.gemini.google/side-panel",
  };
  for (const char* url : kDisallowed) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(features::kGeic,
                                                    {{"geic-guest-url", url}});
    EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty()) << url;
  }
}

TEST_F(GeicEnablingTest, SwitchAcceptsPreProductionHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_EQ(
      ResolveSwitchUrl("https://business-staging.gemini.google/side-panel"),
      GURL("https://business-staging.gemini.google/side-panel"));
}

TEST_F(GeicEnablingTest, RejectsEmbeddedCredentials) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_TRUE(
      ResolvePolicyUrl("https://user:pass@business.gemini.google/side-panel")
          .is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://user@business.gemini.google/side-panel")
                  .is_empty());
  EXPECT_TRUE(
      ResolvePolicyUrl("https://:pass@business.gemini.google/side-panel")
          .is_empty());

  EXPECT_TRUE(
      ResolveSwitchUrl("https://user:pass@business.gemini.google/side-panel")
          .is_empty());
  EXPECT_TRUE(ResolveSwitchUrl("https://user@business.gemini.google/side-panel")
                  .is_empty());
  EXPECT_TRUE(
      ResolveSwitchUrl("https://:pass@business.gemini.google/side-panel")
          .is_empty());

  // Clear the higher-priority sources so that Finch is actually consulted.
  scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
      kGeicGuestURLSwitch);
  SetPolicyUrl("");
  base::test::ScopedFeatureList feature_list2;
  feature_list2.InitAndEnableFeatureWithParameters(
      features::kGeic,
      {{"geic-guest-url",
        "https://user:pass@business.gemini.google/side-panel"}});
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
}

TEST_F(GeicEnablingTest, RejectsDisallowedHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_TRUE(ResolvePolicyUrl("https://gemini.google.com/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("not a url").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://evil.com/").is_empty());
  EXPECT_TRUE(
      ResolvePolicyUrl("https://cloud.google.com.evil.com/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://vertexaisearch.cloud.google.evil.com/")
                  .is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://notcloud.google/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://notgemini.google.com/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("http://business.gemini.google/").is_empty());

  EXPECT_TRUE(
      ResolvePolicyUrl("https://console.cloud.google.com/home").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://storage.cloud.google/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://foo.corp.google.com/").is_empty());
  EXPECT_TRUE(
      ResolvePolicyUrl("https://sub.business.gemini.google/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://sub.gemini.google.com/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://gemini.google/").is_empty());
}

TEST_F(GeicEnablingTest, SwitchAcceptsAllowlistedHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_FALSE(
      ResolveSwitchUrl("https://business.gemini.google/side-panel").is_empty());

  EXPECT_TRUE(
      ResolveSwitchUrl("http://business.gemini.google/side-panel").is_empty());
  EXPECT_TRUE(ResolveSwitchUrl("http://localhost:8080/side-panel").is_empty());
  EXPECT_TRUE(ResolveSwitchUrl("http://127.0.0.1:8080/side-panel").is_empty());
  EXPECT_TRUE(ResolveSwitchUrl("https://localhost:8443/side-panel").is_empty());
  EXPECT_TRUE(
      ResolveSwitchUrl("https://localhost.corp.google.com:10443/side-panel")
          .is_empty());
  EXPECT_TRUE(
      ResolveSwitchUrl("https://custom-dev-host.internal:8443/side-panel")
          .is_empty());

  // `--glic-dev` does not relax switch URL validation.
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      ::switches::kGlicDev);
  EXPECT_TRUE(
      ResolveSwitchUrl("https://custom-dev-host.internal:8443/side-panel")
          .is_empty());
}

TEST_F(GeicEnablingTest, SwitchAcceptsAnyHttpOrHttpsUrlWithAllowAnyGuestUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      kGeicAllowAnyGuestURLSwitch);

  EXPECT_FALSE(
      ResolveSwitchUrl("https://business.gemini.google/side-panel").is_empty());
  EXPECT_FALSE(ResolveSwitchUrl("http://localhost:8080/side-panel").is_empty());
  EXPECT_FALSE(ResolveSwitchUrl("http://127.0.0.1:8080/side-panel").is_empty());
  EXPECT_FALSE(ResolveSwitchUrl("http://[::1]:8080/side-panel").is_empty());
  EXPECT_FALSE(
      ResolveSwitchUrl("https://localhost:8443/side-panel").is_empty());
  EXPECT_FALSE(
      ResolveSwitchUrl("https://localhost.corp.google.com:10443/side-panel")
          .is_empty());
  EXPECT_FALSE(
      ResolveSwitchUrl("http://custom-dev-host.internal:8080/side-panel")
          .is_empty());

  EXPECT_TRUE(ResolveSwitchUrl("ftp://localhost/side-panel").is_empty());
  EXPECT_TRUE(ResolveSwitchUrl("http://user:pass@localhost:8080/side-panel")
                  .is_empty());
  EXPECT_TRUE(ResolveSwitchUrl(
                  "https://user:pass@custom-dev-host.internal:8443/side-panel")
                  .is_empty());
}

TEST_F(GeicEnablingTest, PolicyAndFinchRejectNonAllowlistedHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  // `--glic-dev` and `--geic-allow-any-guest-url` must not relax policy or
  // Finch URL validation.
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      ::switches::kGlicDev);
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      kGeicAllowAnyGuestURLSwitch);

  EXPECT_TRUE(ResolvePolicyUrl("http://localhost:8080/side-panel").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://localhost:8443/side-panel").is_empty());
  EXPECT_TRUE(
      ResolvePolicyUrl("https://localhost.corp.google.com:10443/side-panel")
          .is_empty());

  SetPolicyUrl("");
  base::test::ScopedFeatureList feature_list2;
  feature_list2.InitAndEnableFeatureWithParameters(
      features::kGeic,
      {{"geic-guest-url",
        "https://localhost.corp.google.com:10443/side-panel"}});
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
}

TEST_F(GeicEnablingTest, RejectsDisallowedFinchGuestUrlHost) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url",
          "https://custom-host.corp.google.com/side-panel"}}}},
      {});
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("https://custom-host.corp.google.com/side-panel")
                  .is_empty());
}

TEST_F(GeicEnablingTest, CanonicalizesHomeCidPath) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=abc123"),
            ResolvePolicyUrl("https://business.gemini.google/home/cid/abc123"));
  EXPECT_EQ(
      GURL("https://business.gemini.google/side-panel?configId=abc123"),
      ResolvePolicyUrl("https://business.gemini.google/home/cid/abc123/"));
  EXPECT_EQ(
      GURL("https://business.gemini.google/side-panel?foo=bar&configId=abc"),
      ResolvePolicyUrl("https://business.gemini.google/home/cid/abc?foo=bar"));
  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=abc"),
            ResolvePolicyUrl(
                "https://business.gemini.google/home/cid/abc?configId=other"));
  EXPECT_EQ(
      GURL(
          "https://business.gemini.google/side-panel?configId=abc%26foo%3Dbar"),
      ResolvePolicyUrl("https://business.gemini.google/home/cid/abc&foo=bar"));
  EXPECT_EQ(
      GURL("https://business.gemini.google/side-panel?configId=abc#section"),
      ResolvePolicyUrl("https://business.gemini.google/home/cid/abc#section"));

  // Regional location segments (/<location>/home/cid/<configId>)
  EXPECT_EQ(
      GURL("https://vertexaisearch.cloud.google/side-panel?configId=abc123&"
           "location=us"),
      ResolvePolicyUrl(
          "https://vertexaisearch.cloud.google/us/home/cid/abc123"));
  EXPECT_EQ(
      GURL("https://vertexaisearch.cloud.google/side-panel?configId=abc123&"
           "location=eu"),
      ResolvePolicyUrl(
          "https://vertexaisearch.cloud.google/eu/home/cid/abc123/"));
  EXPECT_EQ(GURL("https://vertexaisearch.cloud.google/side-panel?foo=bar&"
                 "configId=abc&location=eu"),
            ResolvePolicyUrl(
                "https://vertexaisearch.cloud.google/eu/home/cid/abc?foo=bar"));
  EXPECT_EQ(GURL("https://vertexaisearch.cloud.google/side-panel?configId=abc&"
                 "location=eu#section"),
            ResolvePolicyUrl(
                "https://vertexaisearch.cloud.google/eu/home/cid/abc#section"));
}

TEST_F(GeicEnablingTest, LeavesNonHomeCidPathsAlone) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=abc"),
            ResolvePolicyUrl(
                "https://business.gemini.google/side-panel?configId=abc"));
  EXPECT_EQ(GURL("https://business.gemini.google/home/cid/"),
            ResolvePolicyUrl("https://business.gemini.google/home/cid/"));
  EXPECT_EQ(GURL("https://business.gemini.google/home/other/abc"),
            ResolvePolicyUrl("https://business.gemini.google/home/other/abc"));
  EXPECT_EQ(
      GURL("https://vertexaisearch.cloud.google/us/home/cid/"),
      ResolvePolicyUrl("https://vertexaisearch.cloud.google/us/home/cid/"));
  EXPECT_EQ(GURL("https://vertexaisearch.cloud.google/us/home/other/abc"),
            ResolvePolicyUrl(
                "https://vertexaisearch.cloud.google/us/home/other/abc"));
  EXPECT_EQ(
      GURL("https://vertexaisearch.cloud.google/a/b/home/cid/abc"),
      ResolvePolicyUrl("https://vertexaisearch.cloud.google/a/b/home/cid/abc"));
}

TEST_F(GeicEnablingTest, ResolveGeicGuestUrlPrecedence) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url", "https://business.gemini.google/home/cid/finch"}}}},
      {});

  // Without policy or switch, Finch URL is validated and canonicalized.
  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=finch"),
            ResolveGeicGuestUrl(profile()));

  // Policy outranks Finch.
  SetPolicyUrl("https://business.gemini.google/home/cid/policy");
  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=policy"),
            ResolveGeicGuestUrl(profile()));

  // Switch outranks policy.
  SetSwitchUrl("https://business.gemini.google/home/cid/switch");
  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=switch"),
            ResolveGeicGuestUrl(profile()));
}

TEST_F(GeicEnablingTest, DisallowedCandidateDoesNotFallThrough) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url", "https://evil.com/side-panel"}}}},
      {});

  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());

  SetPolicyUrl("https://business.gemini.google/side-panel");
  SetSwitchUrl("https://evil.com/switch");
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());

  // An unparsable switch value doesn't fall through either.
  SetSwitchUrl("not a url");
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
}

TEST_F(GeicEnablingTest, GoogleGroupsFilterEnablesOnlyMatchingProfile) {
  constexpr char kTeamfoodGroupId[] = "657956975164";
  constexpr char kGuestUrl[] =
      "https://ucs-app-autopush.corp.google.com/side-panel";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url", kGuestUrl},
         {variations::internal::kGoogleGroupFeatureParamName,
          kTeamfoodGroupId}}}},
      {});

  TestingPrefServiceSimple local_state;
  local_state.registry()->RegisterDictionaryPref(
      variations::prefs::kVariationsGoogleGroups);

  auto create_profile_with_groups =
      [&local_state](const std::vector<std::string>& groups) {
        TestingProfile::Builder builder;
        builder.AddTestingFactory(
            GoogleGroupsManagerFactory::GetInstance(),
            base::BindLambdaForTesting(
                [&local_state](content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  Profile* p = Profile::FromBrowserContext(context);
                  return std::make_unique<GoogleGroupsManager>(
                      local_state, p->GetPath().BaseName().AsUTF8Unsafe(),
                      *p->GetPrefs());
                }));
        std::unique_ptr<TestingProfile> profile = builder.Build();
        base::ListValue pref_groups_list;
        for (const std::string& group : groups) {
          base::DictValue group_dict;
          group_dict.Set(variations::kDogfoodGroupsSyncPrefGaiaIdKey, group);
          pref_groups_list.Append(std::move(group_dict));
        }
        profile->GetPrefs()->SetList(
#if BUILDFLAG(IS_CHROMEOS)
            variations::kOsDogfoodGroupsSyncPrefName,
#else
            variations::kDogfoodGroupsSyncPrefName,
#endif
            std::move(pref_groups_list));
        // Ensure GoogleGroupsManager is instantiated so it observes the pref.
        GoogleGroupsManagerFactory::GetForBrowserContext(profile.get());
        return profile;
      };

  // Profile in the required Google Group gets GEiC enabled with the Finch URL.
  std::unique_ptr<TestingProfile> corp_profile =
      create_profile_with_groups({kTeamfoodGroupId});
  EXPECT_EQ(ResolveGeicGuestUrl(corp_profile.get()), GURL(kGuestUrl));

  // Personal profile not in the Google Group has GEiC disabled and falls back
  // to empty URL (so consumer GiC is used instead of the corp autopush URL).
  std::unique_ptr<TestingProfile> personal_profile =
      create_profile_with_groups({"111111111"});
  EXPECT_TRUE(ResolveGeicGuestUrl(personal_profile.get()).is_empty());

  // Unsynced profile with no groups also has the Finch guest URL ignored.
  std::unique_ptr<TestingProfile> unsynced_profile =
      create_profile_with_groups({});
  EXPECT_TRUE(ResolveGeicGuestUrl(unsynced_profile.get()).is_empty());

  // A profile not in the Google Group (or unsynced) that has the
  // GeminiEnterpriseUrl admin policy configured still gets GEiC enabled via
  // the policy URL.
  std::unique_ptr<TestingProfile> policy_profile =
      create_profile_with_groups({});
  policy_profile->GetPrefs()->SetString(
      glic::prefs::kGlicGeminiEnterpriseUrl,
      "https://enterprise.gemini.cloud.google/side-panel");
  EXPECT_EQ(ResolveGeicGuestUrl(policy_profile.get()),
            GURL("https://enterprise.gemini.cloud.google/side-panel"));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(GeicEnablingTest, ShouldLatchToGeminiEnterprise) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url", "https://business.gemini.google/side-panel"}}}},
      {});

  TestingProfile* p = profile();
  const GURL kValidGuestUrl("https://business.gemini.google/side-panel");

  SetPolicyWithScope("https://business.gemini.google/home/cid/device",
                     policy::POLICY_SCOPE_MACHINE);

  // Default pref (kAuto) on fresh profile with a valid guest URL -> Enterprise.
  EXPECT_TRUE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // Explicit kAuto with a valid guest URL -> Enterprise.
  p->GetPrefs()->SetInteger(
      glic::prefs::kGlicAssistantSelection,
      static_cast<int>(glic::prefs::AssistantSelection::kAuto));
  EXPECT_TRUE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // kConsumer with a machine-scoped policy on a personal profile -> Consumer.
  p->GetPrefs()->SetInteger(
      glic::prefs::kGlicAssistantSelection,
      static_cast<int>(glic::prefs::AssistantSelection::kConsumer));
  EXPECT_FALSE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // kConsumer on a managed profile is ignored -> Enterprise.
  p->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_TRUE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));
  p->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  EXPECT_FALSE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // kConsumer with a user-scoped policy is ignored -> Enterprise.
  SetPolicyWithScope("https://business.gemini.google/home/cid/user",
                     policy::POLICY_SCOPE_USER);
  EXPECT_TRUE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));
  SetPolicyWithScope("https://business.gemini.google/home/cid/device",
                     policy::POLICY_SCOPE_MACHINE);
  EXPECT_FALSE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // kConsumer when consumer Gemini is disabled by policy -> Enterprise.
  p->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      static_cast<int>(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  EXPECT_TRUE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));
  p->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      static_cast<int>(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  EXPECT_FALSE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // kConsumer is ignored when GEiC is resolved via --geic-guest-url switch ->
  // Enterprise.
  SetSwitchUrl("https://business.gemini.google/cli-panel");
  EXPECT_TRUE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));
  scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
      kGeicGuestURLSwitch);
  EXPECT_FALSE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // kConsumer is ignored when GEiC is resolved via Finch (no policy set) ->
  // Enterprise.
  policy_provider_.UpdateChromePolicy(policy::PolicyMap());
  SetPolicyUrl("");
  EXPECT_TRUE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));
  SetPolicyWithScope("https://business.gemini.google/home/cid/device",
                     policy::POLICY_SCOPE_MACHINE);
  EXPECT_FALSE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // kEnterprise with a valid guest URL -> Enterprise.
  p->GetPrefs()->SetInteger(
      glic::prefs::kGlicAssistantSelection,
      static_cast<int>(glic::prefs::AssistantSelection::kEnterprise));
  EXPECT_TRUE(ShouldLatchToGeminiEnterprise(p, kValidGuestUrl));

  // Empty guest URL -> Consumer regardless of selection.
  EXPECT_FALSE(ShouldLatchToGeminiEnterprise(p, GURL()));

  // Null BrowserContext -> Consumer.
  EXPECT_FALSE(ShouldLatchToGeminiEnterprise(nullptr, kValidGuestUrl));
}

TEST_F(GeicEnablingTest, CanSwitchBetweenGeminiProvidersInSettings) {
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(nullptr));
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(profile()));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url", "https://business.gemini.google/side-panel"}}}},
      {});

  // Finch URL alone does not allow switching in settings.
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(profile()));

  // User-scoped policy does not allow switching in settings.
  SetPolicyWithScope("https://business.gemini.google/home/cid/user",
                     policy::POLICY_SCOPE_USER);
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(profile()));

  // Machine-scoped policy with a disallowed URL does not allow switching.
  SetPolicyWithScope("https://evil.com/side-panel",
                     policy::POLICY_SCOPE_MACHINE);
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(profile()));

  // Machine-scoped policy with a valid URL allows switching on a personal
  // (unmanaged) profile.
  SetPolicyWithScope("https://business.gemini.google/home/cid/device",
                     policy::POLICY_SCOPE_MACHINE);
  EXPECT_TRUE(CanSwitchBetweenGeminiProvidersInSettings(profile()));

  // Off-the-record profiles return false.
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  // On a managed profile, `CanSwitchBetweenGeminiProvidersInSettings` returns
  // false even when the policy is machine-scoped.
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(profile()));
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  EXPECT_TRUE(CanSwitchBetweenGeminiProvidersInSettings(profile()));

  // When consumer Gemini is disabled by admin policy (`GeminiSettings`), the
  // switch is not offered.
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      static_cast<int>(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(profile()));
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      static_cast<int>(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  EXPECT_TRUE(CanSwitchBetweenGeminiProvidersInSettings(profile()));

  // Command-line switch takes precedence over the device policy.
  SetSwitchUrl("https://business.gemini.google/cli-panel");
  EXPECT_FALSE(CanSwitchBetweenGeminiProvidersInSettings(profile()));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

TEST_F(GeicEnablingTest, GetGeicSkillsUrl) {
  auto get_skills_url_for_guest_url = [](const std::string& guest_url) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kGeic,
           {{"enabled", "true"}, {"geic-guest-url", guest_url}}}},
        {});
    TestingProfile profile;
    return GetGeicSkillsUrl(&profile);
  };

  // Extracts `configId` into `/home/cid/<configId>/r/skills`, strips
  // `configId`, `location`, and `hl`, and preserves other query parameters.
  EXPECT_EQ(
      get_skills_url_for_guest_url(
          "https://business.gemini.google/side-panel?"
          "configId=abc123&location=global&hl=en&mods=widget_staging_api_mod"),
      GURL("https://business.gemini.google/home/cid/abc123/r/skills?"
           "mods=widget_staging_api_mod"));

  // Falls back to `/r/skills` and clears the query string when `configId` is
  // absent and no other query parameters remain.
  EXPECT_EQ(get_skills_url_for_guest_url(
                "https://business.gemini.google/side-panel?location=us&hl=fr"),
            GURL("https://business.gemini.google/r/skills"));

  // Returns an empty GURL when the resolved guest URL is empty/invalid.
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ::switches::kGlicGuestURL, "");
  EXPECT_TRUE(GetGeicSkillsUrl(profile()).is_empty());
}
}  // namespace
}  // namespace geic
