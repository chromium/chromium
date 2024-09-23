// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_config.h"
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class SegmentationPlatformConfigTest : public testing::Test {
 public:
  SegmentationPlatformConfigTest() = default;
  ~SegmentationPlatformConfigTest() override = default;

  void EnableFeaturesWithParams(
      const std::vector<base::test::FeatureRefAndParams>& enabled_features) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    // Activate each field trial so that these trials are considered as Active
    // groups. the FieldTrialList currently does not consider force enabled
    // trials as "active" by default.
    for (const auto& feature : enabled_features) {
      constexpr char kTrialPrefix[] = "scoped_feature_list_trial_for_";
      auto* field_trial = base::FieldTrialList::Find(
          base::StrCat({kTrialPrefix, feature.feature->name}));
      ASSERT_TRUE(field_trial);
      field_trial->Activate();
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SegmentationPlatformConfigTest, GetSegmentationPlatformConfig) {
  std::vector<std::unique_ptr<Config>> configs =
      GetSegmentationPlatformConfig(nullptr, nullptr);
  for (const auto& config : configs) {
    EXPECT_TRUE(config.get());
  }
}

TEST_F(SegmentationPlatformConfigTest, EmptyFeatures) {
  std::vector<std::unique_ptr<Config>> configs;
  AppendConfigsFromExperiments(configs);
  EXPECT_TRUE(configs.empty());
}

TEST_F(SegmentationPlatformConfigTest, BadFormat) {
  std::vector<base::test::FeatureRefAndParams> features;
  features.push_back(base::test::FeatureRefAndParams(
      features::kSegmentationPlatformLowEngagementFeature,
      {{"segmentation_platform_add_config_param", "bad_json"}}));
  EnableFeaturesWithParams(features);
  std::vector<std::unique_ptr<Config>> configs;
  AppendConfigsFromExperiments(configs);
  EXPECT_TRUE(configs.empty());
}

MATCHER_P2(DoesConfigMatch, key, segment_ids, "") {
  if (segment_ids.size() != arg->segments.size())
    return false;
  for (auto segment : segment_ids) {
    if (arg->segments.count(segment) == 0)
      return false;
  }
  return arg->segmentation_key == key;
}

TEST_F(SegmentationPlatformConfigTest, MultipleConfigs) {
  constexpr char kValidConfig1[] = R"({
    "segmentation_key": "test_key",
    "segmentation_uma_name": "TestKey",
    "segments": {
      "4" : {"segment_uma_name" : "LowEngagement"},
      "6" : {"segment_uma_name" : "HighEngagement"},
      "5" : {"segment_uma_name" : "MediumEngagement"}
    },
    "segment_selection_ttl_days": 10
  })";
  constexpr char kValidConfig2[] = R"({
      "segmentation_key": "test_key1",
      "segmentation_uma_name": "TestKey1",
      "segments": {
        "10" : {"segment_uma_name" : "FeedUser"}
      },
      "segment_selection_ttl_days": 10,
      "unknown_segment_selection_ttl_days": 14
  })";
  const base::flat_set<proto::SegmentId> segment_ids1 = {
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_VOICE};
  const base::flat_set<proto::SegmentId> segment_ids2 = {
      proto::SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_DUMMY};

  std::vector<base::test::FeatureRefAndParams> features;
  features.push_back(base::test::FeatureRefAndParams(
      features::kSegmentationPlatformLowEngagementFeature,
      {{"segmentation_platform_add_config_param", "bad_json"}}));
  features.push_back(base::test::FeatureRefAndParams(
      features::kSegmentationPlatformFeedSegmentFeature,
      {{"segmentation_platform_add_config_param", kValidConfig1}}));
  features.push_back(base::test::FeatureRefAndParams(
      features::kShoppingUserSegmentFeature,
      {{"segmentation_platform_add_config_param", kValidConfig2}}));
  EnableFeaturesWithParams(features);

  std::vector<std::unique_ptr<Config>> configs;
  AppendConfigsFromExperiments(configs);
  ASSERT_EQ(2u, configs.size());
  EXPECT_THAT(configs, testing::UnorderedElementsAre(
                           DoesConfigMatch("test_key", segment_ids1),
                           DoesConfigMatch("test_key1", segment_ids2)));
}

}  // namespace segmentation_platform
