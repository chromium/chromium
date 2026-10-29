// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(GlicProviderSpecImplTest, GeminiSupportsSsrFollowsGlicSsr) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kGlicSsr);
    EXPECT_TRUE(
        CreateGlicProviderSpec(GlicProvider::kGemini, {})->SupportsSsr());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kGlicSsr);
    EXPECT_FALSE(
        CreateGlicProviderSpec(GlicProvider::kGemini, {})->SupportsSsr());
  }
}

TEST(GlicProviderSpecImplTest, GeminiEnterpriseSupportsSsrFollowsGeicSsr) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGeicSsr}, {features::kGlicSsr});
    EXPECT_TRUE(CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
                    ->SupportsSsr());
  }
  // kGlicSsr only applies to consumer Gemini.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGlicSsr}, {features::kGeicSsr});
    EXPECT_FALSE(CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {})
                     ->SupportsSsr());
  }
}

}  // namespace
}  // namespace glic
