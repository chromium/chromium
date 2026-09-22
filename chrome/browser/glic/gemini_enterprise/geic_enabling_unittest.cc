// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

class GeicEnablingTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
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

}  // namespace
}  // namespace geic
