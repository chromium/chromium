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
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/google_groups_manager.h"
#include "components/variations/service/google_groups_manager_prefs.h"
#include "components/variations/variations_seed_processor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

class GeicEnablingTest : public testing::Test {
 public:
  void TearDown() override { profile_.reset(); }

 protected:
  TestingProfile* profile() {
    if (!profile_) {
      profile_ = std::make_unique<TestingProfile>();
    }
    return profile_.get();
  }

  void SetPolicyUrl(const std::string& url) {
    profile()->GetPrefs()->SetString(glic::prefs::kGlicGeminiEnterpriseUrl,
                                     url);
  }

  void SetSwitchUrl(const std::string& url) {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        kGeicGuestURLSwitch, url);
  }

  GURL ResolvePolicyUrl(const std::string& url) {
    SetPolicyUrl(url);
    return ResolveGeicGuestUrl(profile());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedCommandLine scoped_command_line_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(GeicEnablingTest, DefaultStateIsDisabled) {
  EXPECT_FALSE(IsGeicEnabledByFeature());
  EXPECT_TRUE(ResolveGeicGuestUrl(profile()).is_empty());
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
}

TEST_F(GeicEnablingTest, RejectsDisallowedHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

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
      ResolvePolicyUrl("https://ucs-app-autopush.corp.google.com/side-panel")
          .is_empty());
}

TEST_F(GeicEnablingTest, AcceptsLocalhostOverHttpOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  // Localhost is rejected without --glic-dev.
  EXPECT_TRUE(ResolvePolicyUrl("http://localhost:8080/side-panel").is_empty());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);

  EXPECT_FALSE(ResolvePolicyUrl("http://localhost:8080/side-panel").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("https://localhost/").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("http://127.0.0.1:8080/").is_empty());

  EXPECT_TRUE(ResolvePolicyUrl("ws://localhost/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("chrome://localhost/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("ftp://localhost/").is_empty());
}

TEST_F(GeicEnablingTest, AcceptsFinchGuestUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url",
          "https://ucs-app-autopush.corp.google.com/side-panel"}}}},
      {});
  EXPECT_EQ(ResolveGeicGuestUrl(profile()),
            GURL("https://ucs-app-autopush.corp.google.com/side-panel"));
  EXPECT_FALSE(
      ResolvePolicyUrl("https://ucs-app-autopush.corp.google.com/side-panel")
          .is_empty());
}

TEST_F(GeicEnablingTest, AcceptsGlicDevSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kGlicDev);

  EXPECT_FALSE(
      ResolvePolicyUrl("https://localhost.corp.google.com:10443/side-panel")
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
         {"geic-guest-url", "http://evil.com/side-panel"}}}},
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

}  // namespace
}  // namespace geic
