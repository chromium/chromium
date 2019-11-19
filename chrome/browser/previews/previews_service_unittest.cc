// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_service.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Class to test the validity of the callback passed to PreviewsDeciderImpl from
// PreviewsService.
class PreviewsServiceTest : public testing::Test {
 public:
  PreviewsServiceTest() {}
  ~PreviewsServiceTest() override {}

  void TearDown() override { variations::testing::ClearAllVariationParams(); }
};

}  // namespace

TEST_F(PreviewsServiceTest, TestOfflineFieldTrialNotSet) {
  blacklist::BlacklistData::AllowedTypesAndVersions allowed_types_and_versions =
      PreviewsService::GetAllowedPreviews();
  EXPECT_NE(allowed_types_and_versions.find(
                static_cast<int>(previews::PreviewsType::OFFLINE)),
            allowed_types_and_versions.end());
}

TEST_F(PreviewsServiceTest, TestOfflineFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      previews::features::kOfflinePreviews);

  blacklist::BlacklistData::AllowedTypesAndVersions allowed_types_and_versions =
      PreviewsService::GetAllowedPreviews();
  EXPECT_EQ(allowed_types_and_versions.find(
                static_cast<int>(previews::PreviewsType::OFFLINE)),
            allowed_types_and_versions.end());
}

TEST_F(PreviewsServiceTest, TestLitePageNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews} /* enabled features */,
      {data_reduction_proxy::features::
           kDataReductionProxyDecidesTransform} /* disabled features */);
  blacklist::BlacklistData::AllowedTypesAndVersions allowed_types_and_versions =
      PreviewsService::GetAllowedPreviews();
  EXPECT_EQ(allowed_types_and_versions.find(
                static_cast<int>(previews::PreviewsType::LITE_PAGE)),
            allowed_types_and_versions.end());
}

TEST_F(PreviewsServiceTest, TestLitePageProxyDecidesTransform) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {previews::features::kPreviews,
       data_reduction_proxy::features::kDataReductionProxyDecidesTransform},
      {});
  blacklist::BlacklistData::AllowedTypesAndVersions allowed_types_and_versions =
      PreviewsService::GetAllowedPreviews();
  EXPECT_NE(allowed_types_and_versions.find(
                static_cast<int>(previews::PreviewsType::LITE_PAGE)),
            allowed_types_and_versions.end());
}

TEST_F(PreviewsServiceTest, TestNoScriptPreviewsEnabledByFeature) {
#if !defined(OS_ANDROID)
  // For non-android, default is disabled.
  blacklist::BlacklistData::AllowedTypesAndVersions allowed_types_and_versions =
      PreviewsService::GetAllowedPreviews();
  EXPECT_EQ(allowed_types_and_versions.find(
                static_cast<int>(previews::PreviewsType::NOSCRIPT)),
            allowed_types_and_versions.end());
#endif  // defined(OS_ANDROID)

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kNoScriptPreviews);
  blacklist::BlacklistData::AllowedTypesAndVersions
      allowed_types_and_versions2 = PreviewsService::GetAllowedPreviews();
  EXPECT_NE(allowed_types_and_versions2.find(
                static_cast<int>(previews::PreviewsType::NOSCRIPT)),
            allowed_types_and_versions2.end());
}

TEST_F(PreviewsServiceTest, TestDeferAllScriptPreviewsEnabledByFeature) {
#if !defined(OS_ANDROID)
  // For non-android, default is disabled.
  blacklist::BlacklistData::AllowedTypesAndVersions allowed_types_and_versions =
      PreviewsService::GetAllowedPreviews();
  EXPECT_EQ(allowed_types_and_versions.find(
                static_cast<int>(previews::PreviewsType::DEFER_ALL_SCRIPT)),
            allowed_types_and_versions.end());
#endif  // defined(OS_ANDROID)

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kDeferAllScriptPreviews);
  blacklist::BlacklistData::AllowedTypesAndVersions
      allowed_types_and_versions2 = PreviewsService::GetAllowedPreviews();
  EXPECT_NE(allowed_types_and_versions2.find(
                static_cast<int>(previews::PreviewsType::DEFER_ALL_SCRIPT)),
            allowed_types_and_versions2.end());
}

TEST_F(PreviewsServiceTest, HasURLRedirectCycle) {
  base::MRUCache<GURL, GURL> redirect_history(100u);

  // URL redirect cycle of length 3.
  redirect_history.Put(GURL("https://a.com/"), GURL("https://b.com/"));
  redirect_history.Put(GURL("https://b.com/"), GURL("https://c.com/"));
  redirect_history.Put(GURL("https://c.com/"), GURL("https://a.com/"));

  // Not a cycle.
  redirect_history.Put(GURL("https://m.com/"), GURL("https://n.com/"));
  redirect_history.Put(GURL("https://n.com/"), GURL("https://o.com/"));
  redirect_history.Put(GURL("https://o.com/"), GURL("https://p.com/"));

  // URL redirect cycle of length 3.
  redirect_history.Put(GURL("https://q.com/"), GURL("https://r.com/"));
  redirect_history.Put(GURL("https://r.com/"), GURL("https://s.com/"));
  redirect_history.Put(GURL("https://s.com/"), GURL("https://s.com/"));

  // URL redirect cycle of length 2.
  redirect_history.Put(GURL("https://x.com/"), GURL("https://y.com/"));
  redirect_history.Put(GURL("https://y.com/"), GURL("https://x.com/"));

  const struct {
    const char* start_url;
    bool expect_redirect_cycle;
  } kTestCases[] = {
      {"https://a.com/", true},  {"https://b.com/", true},
      {"https://c.com/", true},  {"https://d.com/", false},
      {"https://m.com/", false}, {"https://n.com/", false},
      {"https://o.com/", false}, {"https://p.com/", false},
      {"https://q.com/", true},  {"https://r.com/", true},
      {"https://s.com/", true},  {"https://x.com/", true},
      {"https://y.com/", true},
  };

  for (const auto& test : kTestCases) {
    EXPECT_EQ(test.expect_redirect_cycle,
              PreviewsService::HasURLRedirectCycle(GURL(test.start_url),
                                                   redirect_history))
        << " test.start_url=" << test.start_url;
  }
}
