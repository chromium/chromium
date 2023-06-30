// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion {

namespace {

class CompanionCoreUtilsTest : public testing::Test {};

TEST_F(CompanionCoreUtilsTest, HomepageURLForCompanion) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::internal::kSidePanelCompanion);

  EXPECT_EQ("https://lens.google.com/companion", GetHomepageURLForCompanion());
}

TEST_F(CompanionCoreUtilsTest,
       HomepageURLForCompanionWithParams_kSidePanelCompanion) {
  base::test::ScopedFeatureList scoped_list;

  base::FieldTrialParams params;
  params.insert({"companion-homepage-url", "https://foo.com/bar"});
  scoped_list.InitAndEnableFeatureWithParameters(
      features::internal::kSidePanelCompanion, params);

  EXPECT_EQ("https://foo.com/bar", GetHomepageURLForCompanion());
}

TEST_F(CompanionCoreUtilsTest,
       HomepageURLForCompanionWithParams_kSidePanelCompanion2) {
  base::test::ScopedFeatureList scoped_list;

  base::FieldTrialParams params;
  params.insert({"companion-homepage-url", "https://foo.com/bar"});
  scoped_list.InitAndEnableFeatureWithParameters(
      features::internal::kSidePanelCompanion2, params);

  EXPECT_EQ("https://foo.com/bar", GetHomepageURLForCompanion());
}

TEST_F(CompanionCoreUtilsTest, ImageUploadURLForCompanion) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(features::internal::kSidePanelCompanion);

  EXPECT_EQ("https://lens.google.com/upload", GetImageUploadURLForCompanion());
}

}  // namespace

}  // namespace companion
