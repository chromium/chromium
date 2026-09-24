// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

class GeicEnablingTest : public testing::Test {
 public:
  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }
  void TearDown() override { profile_.reset(); }

 protected:
  void SetPolicyUrl(const std::string& url) {
    base::DictValue settings;
    settings.Set("url", url);
    profile_->GetPrefs()->SetDict(glic::prefs::kGlicGeminiEnterpriseSettings,
                                  std::move(settings));
  }

  GURL ResolvePolicyUrl(const std::string& url) {
    SetPolicyUrl(url);
    return GetGeicGuestUrl(profile_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(GeicEnablingTest, DefaultStateIsDisabled) {
  EXPECT_FALSE(IsGeicEnabled());
  EXPECT_TRUE(GetGeicGuestUrl().is_empty());
}

TEST_F(GeicEnablingTest, FinchEnabledWithDefaults) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_TRUE(IsGeicEnabled());
  EXPECT_TRUE(GetGeicGuestUrl().is_empty());
}

TEST_F(GeicEnablingTest, FinchEnabledWithExplicitDisabledParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(features::kGeic,
                                                  {{"enabled", "false"}});

  EXPECT_FALSE(IsGeicEnabled());
  EXPECT_TRUE(GetGeicGuestUrl().is_empty());
}

TEST_F(GeicEnablingTest, FinchEnabledWithCustomGuestUrlParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGeic,
      {{"enabled", "true"},
       {"geic-guest-url", "https://business.gemini.google/custom-panel"}});

  EXPECT_TRUE(IsGeicEnabled());
  EXPECT_EQ(GetGeicGuestUrl(),
            GURL("https://business.gemini.google/custom-panel"));
}

TEST_F(GeicEnablingTest, FlagExplicitlyDisabledOverridesFinch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGeic);

  EXPECT_FALSE(IsGeicEnabled());
  EXPECT_TRUE(GetGeicGuestUrl().is_empty());
}

TEST_F(GeicEnablingTest, FlagExplicitlyEnabledBypassesChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_TRUE(IsGeicEnabled());
  EXPECT_TRUE(GetGeicGuestUrl().is_empty());
}

TEST_F(GeicEnablingTest, CommandLineSwitchOverridesGuestUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kGeicGuestURLSwitch, "https://business.gemini.google/cli-panel");

  EXPECT_EQ(GetGeicGuestUrl(),
            GURL("https://business.gemini.google/cli-panel"));
}

TEST_F(GeicEnablingTest, IsGlicNoWebviewEnabled_BothOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({},
                                {features::kGlicNoWebview, features::kGeic});

  EXPECT_FALSE(features::IsGlicNoWebviewEnabled());
}

TEST_F(GeicEnablingTest, IsGlicNoWebviewEnabled_GlicNoWebviewOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kGlicNoWebview}, {features::kGeic});

  EXPECT_TRUE(features::IsGlicNoWebviewEnabled());
}

TEST_F(GeicEnablingTest, IsGlicNoWebviewEnabled_GeicOn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic, {{"enabled", "true"}}}}, {features::kGlicNoWebview});

  EXPECT_TRUE(features::IsGlicNoWebviewEnabled());
}

TEST_F(GeicEnablingTest, IsGlicNoWebviewEnabled_GeicOnButParamFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic, {{"enabled", "false"}}}}, {features::kGlicNoWebview});

  EXPECT_FALSE(features::IsGlicNoWebviewEnabled());
}

TEST_F(GeicEnablingTest, AcceptsAllowedHosts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_FALSE(ResolvePolicyUrl("https://business.gemini.google/").is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://vertexaisearch.cloud.google.com/home/cid/abc")
          .is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://vertexaisearch.cloud.google/home/cid/abc")
          .is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://business.gemini.google/side-panel").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("https://gemini.google.com/app").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("https://cloud.google.com/home").is_empty());
  EXPECT_FALSE(
      ResolvePolicyUrl("https://console.cloud.google.com/home").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("https://foo.corp.google.com/").is_empty());
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
}

TEST_F(GeicEnablingTest, AcceptsLocalhostOverHttpOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGeic);

  EXPECT_FALSE(ResolvePolicyUrl("http://localhost:8080/side-panel").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("https://localhost/").is_empty());
  EXPECT_FALSE(ResolvePolicyUrl("http://127.0.0.1:8080/").is_empty());

  EXPECT_TRUE(ResolvePolicyUrl("ws://localhost/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("chrome://localhost/").is_empty());
  EXPECT_TRUE(ResolvePolicyUrl("ftp://localhost/").is_empty());
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

TEST_F(GeicEnablingTest, GetGeicGuestUrlPrecedence) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url", "https://business.gemini.google/home/cid/finch"}}}},
      {});

  // Without policy or switch, Finch URL is validated and canonicalized.
  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=finch"),
            GetGeicGuestUrl(profile_.get()));

  // Policy outranks Finch.
  SetPolicyUrl("https://business.gemini.google/home/cid/policy");
  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=policy"),
            GetGeicGuestUrl(profile_.get()));

  // Switch outranks policy.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kGeicGuestURLSwitch, "https://business.gemini.google/home/cid/switch");
  EXPECT_EQ(GURL("https://business.gemini.google/side-panel?configId=switch"),
            GetGeicGuestUrl(profile_.get()));
}

TEST_F(GeicEnablingTest, DisallowedCandidateDoesNotFallThrough) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic,
        {{"enabled", "true"},
         {"geic-guest-url", "https://evil.com/side-panel"}}}},
      {});

  EXPECT_TRUE(GetGeicGuestUrl(profile_.get()).is_empty());

  SetPolicyUrl("https://business.gemini.google/side-panel");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kGeicGuestURLSwitch, "https://evil.com/switch");
  EXPECT_TRUE(GetGeicGuestUrl(profile_.get()).is_empty());
}

}  // namespace
}  // namespace geic
