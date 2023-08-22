// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_feature.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MediaRouterFeatureTest, GetCastAllowAllIPsPref) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterBooleanPref(
      prefs::kMediaRouterCastAllowAllIPs, false);
  EXPECT_FALSE(GetCastAllowAllIPsPref(pref_service.get()));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCastAllowAllIPsFeature);
  EXPECT_TRUE(GetCastAllowAllIPsPref(pref_service.get()));

  pref_service->SetManagedPref(prefs::kMediaRouterCastAllowAllIPs,
                               std::make_unique<base::Value>(true));
  EXPECT_TRUE(GetCastAllowAllIPsPref(pref_service.get()));

  pref_service->SetManagedPref(prefs::kMediaRouterCastAllowAllIPs,
                               std::make_unique<base::Value>(false));
  EXPECT_FALSE(GetCastAllowAllIPsPref(pref_service.get()));
}

TEST(MediaRouterFeatureTest, GetReceiverIdHashToken) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  pref_service->registry()->RegisterStringPref(
      prefs::kMediaRouterReceiverIdHashToken, "");

  std::string token = GetReceiverIdHashToken(pref_service.get());
  EXPECT_FALSE(token.empty());

  // Token stays the same on subsequent invocation.
  EXPECT_EQ(token, GetReceiverIdHashToken(pref_service.get()));
}

TEST(MediaRouterFeatureTest, GetCastMirroringPlayoutDelay) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams feature_params;
  feature_params[kCastMirroringPlayoutDelayMs.name] = "100";
  feature_list.InitAndEnableFeatureWithParameters(kCastMirroringPlayoutDelay,
                                                  feature_params);
  EXPECT_TRUE(GetCastMirroringPlayoutDelay().has_value());
  EXPECT_EQ(GetCastMirroringPlayoutDelay().value(), base::Milliseconds(100));

  // Incorrect values are ignored.
  feature_list.Reset();
  feature_params[kCastMirroringPlayoutDelayMs.name] = "0";
  feature_list.InitAndEnableFeatureWithParameters(kCastMirroringPlayoutDelay,
                                                  feature_params);
  EXPECT_FALSE(GetCastMirroringPlayoutDelay().has_value());

  feature_list.Reset();
  feature_params[kCastMirroringPlayoutDelayMs.name] = "2000";
  feature_list.InitAndEnableFeatureWithParameters(kCastMirroringPlayoutDelay,
                                                  feature_params);
  EXPECT_FALSE(GetCastMirroringPlayoutDelay().has_value());
}

TEST(MediaRouterFeatureTest, GetCastMirroringPlayoutDelayCommandLine) {
  base::test::ScopedFeatureList feature_list;
  // Test that an invalid switch is not returned.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kCastMirroringTargetPlayoutDelay,
                                  "foo");
  EXPECT_FALSE(GetCastMirroringPlayoutDelay().has_value());

  base::TimeDelta expected_delay = base::Milliseconds(200);
  // Test that valid values are passed.
  command_line->AppendSwitchASCII(switches::kCastMirroringTargetPlayoutDelay,
                                  "200");
  EXPECT_EQ(GetCastMirroringPlayoutDelay().value(), expected_delay);

  // Test that command line takes precedence over feature.
  base::FieldTrialParams feature_params;
  feature_params[kCastMirroringPlayoutDelayMs.name] = "500";
  feature_list.InitAndEnableFeatureWithParameters(kCastMirroringPlayoutDelay,
                                                  feature_params);
  ASSERT_NE(base::Milliseconds(kCastMirroringPlayoutDelayMs.Get()),
            expected_delay);
  EXPECT_EQ(GetCastMirroringPlayoutDelay().value(), base::Milliseconds(200));
}

class MediaRouterEnabledTest : public ::testing::Test {
 public:
  MediaRouterEnabledTest() = default;
  MediaRouterEnabledTest(const MediaRouterEnabledTest&) = delete;
  ~MediaRouterEnabledTest() override = default;
  MediaRouterEnabledTest& operator=(const MediaRouterEnabledTest&) = delete;
  void SetUp() override { ClearMediaRouterStoredPrefsForTesting(); }
  void TearDown() override { ClearMediaRouterStoredPrefsForTesting(); }

 protected:
  content::BrowserTaskEnvironment test_environment;
  TestingProfile enabled_profile;
  TestingProfile disabled_profile;
};

TEST_F(MediaRouterEnabledTest, TestEnabledByPolicy_SameOTR) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMediaRouterOTRInstance);

  enabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(true));
  EXPECT_TRUE(MediaRouterEnabled(&enabled_profile));

  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&enabled_profile);
  EXPECT_TRUE(MediaRouterEnabled(incognito_profile));

  enabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(false));
  // Runtime changes are not supported.
  EXPECT_TRUE(MediaRouterEnabled(&enabled_profile));
  // Should remain enabled for incognito too.
  EXPECT_TRUE(MediaRouterEnabled(incognito_profile));
}

TEST_F(MediaRouterEnabledTest, TestEnabledByPolicy_SeparateOTR) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMediaRouterOTRInstance);

  enabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(true));
  EXPECT_TRUE(MediaRouterEnabled(&enabled_profile));

  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&enabled_profile);
  EXPECT_TRUE(MediaRouterEnabled(incognito_profile));

  enabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(false));
  // Runtime changes are not supported.
  EXPECT_TRUE(MediaRouterEnabled(&enabled_profile));
  // Should remain enabled for incognito too.
  EXPECT_TRUE(MediaRouterEnabled(incognito_profile));
}

TEST_F(MediaRouterEnabledTest, TestDisabledByPolicy_SameOTR) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMediaRouterOTRInstance);

  disabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(false));
  EXPECT_FALSE(MediaRouterEnabled(&disabled_profile));

  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&disabled_profile);
  EXPECT_FALSE(MediaRouterEnabled(incognito_profile));

  disabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(true));
  // Runtime changes are not supported.
  EXPECT_FALSE(MediaRouterEnabled(&disabled_profile));
  // Should remain disabled for incognito too.
  EXPECT_FALSE(MediaRouterEnabled(incognito_profile));
}

TEST_F(MediaRouterEnabledTest, TestDisabledByPolicy_SeparateOTR) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMediaRouterOTRInstance);

  disabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(false));
  EXPECT_FALSE(MediaRouterEnabled(&disabled_profile));

  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&disabled_profile);
  EXPECT_FALSE(MediaRouterEnabled(incognito_profile));

  disabled_profile.GetTestingPrefService()->SetManagedPref(
      ::prefs::kEnableMediaRouter, std::make_unique<base::Value>(true));
  // Runtime changes are not supported.
  EXPECT_FALSE(MediaRouterEnabled(&disabled_profile));
  // Should remain disabled for incognito too.
  EXPECT_FALSE(MediaRouterEnabled(incognito_profile));
}

}  // namespace media_router
