// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/client_id_backup_correctness_fix_field_trial.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "components/metrics/metrics_features.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

void VerifyChannelGroupAssignment(version_info::Channel channel,
                                  double entropy_value,
                                  std::string_view expected_group,
                                  bool expected_enabled) {
  base::test::ScopedFeatureList empty_lists;
  empty_lists.InitWithNullFeatureAndFieldTrialLists();
  base::FieldTrialList field_trial_list;

  auto feature_list = std::make_unique<base::FeatureList>();
  variations::MockEntropyProviders entropy_providers(
      {.low_entropy = entropy_value});
  CreateClientIdBackupCorrectnessFixFieldTrialForTesting(
      *feature_list, entropy_providers.default_entropy(), channel);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  EXPECT_TRUE(base::FieldTrialList::TrialExists(
      kClientIdBackupCorrectnessFixTrialName));
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(
      kClientIdBackupCorrectnessFixTrialName));
  EXPECT_EQ(
      base::FeatureList::IsEnabled(features::kClientIdBackupCorrectnessFix),
      expected_enabled);

  std::vector<variations::ActiveGroupId> active_groups;
  variations::GetFieldTrialActiveGroupIds(/*suffix=*/"", &active_groups);
  if (expected_group != "Default") {
    EXPECT_TRUE(variations::ContainsTrialAndGroupName(
        active_groups, kClientIdBackupCorrectnessFixTrialName, expected_group));
  } else {
    EXPECT_FALSE(variations::ContainsTrialName(
        active_groups, kClientIdBackupCorrectnessFixTrialName));
  }
  EXPECT_EQ(base::FieldTrialList::FindFullName(
                kClientIdBackupCorrectnessFixTrialName),
            expected_group);
}

std::string ChannelParamName(
    const ::testing::TestParamInfo<version_info::Channel>& info) {
  return std::string(version_info::GetChannelString(info.param));
}

class ClientIdBackupCorrectnessFixFieldTrial50PercentTest
    : public ::testing::TestWithParam<version_info::Channel> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ClientIdBackupCorrectnessFixFieldTrial50PercentTest,
                         ::testing::Values(version_info::Channel::CANARY,
                                           version_info::Channel::DEV),
                         &ChannelParamName);

TEST_P(ClientIdBackupCorrectnessFixFieldTrial50PercentTest,
       Assigns50PercentGroups) {
  const version_info::Channel channel = GetParam();
  VerifyChannelGroupAssignment(
      channel, /*entropy_value=*/variations::kAlwaysUseFirstGroup,
      "EnabledClientSide",
      /*expected_enabled=*/true);
  VerifyChannelGroupAssignment(channel, /*entropy_value=*/0.49,
                               "EnabledClientSide",
                               /*expected_enabled=*/true);
  VerifyChannelGroupAssignment(channel, /*entropy_value=*/0.50,
                               "DisabledClientSide",
                               /*expected_enabled=*/false);
  VerifyChannelGroupAssignment(
      channel, /*entropy_value=*/variations::kAlwaysUseLastGroup,
      "DisabledClientSide",
      /*expected_enabled=*/false);
}

class ClientIdBackupCorrectnessFixFieldTrialDefaultTest
    : public ::testing::TestWithParam<version_info::Channel> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ClientIdBackupCorrectnessFixFieldTrialDefaultTest,
                         ::testing::Values(version_info::Channel::BETA,
                                           version_info::Channel::STABLE,
                                           version_info::Channel::UNKNOWN),
                         &ChannelParamName);

TEST_P(ClientIdBackupCorrectnessFixFieldTrialDefaultTest, AssignsDefaultGroup) {
  const version_info::Channel channel = GetParam();
  VerifyChannelGroupAssignment(
      channel, /*entropy_value=*/variations::kAlwaysUseFirstGroup, "Default",
      /*expected_enabled=*/false);
  VerifyChannelGroupAssignment(
      channel, /*entropy_value=*/variations::kAlwaysUseLastGroup, "Default",
      /*expected_enabled=*/false);
}

TEST(ClientIdBackupCorrectnessFixFieldTrialTest,
     RespectsExistingFeatureOverrideAndRepeatedCalls) {
  base::test::ScopedFeatureList empty_lists;
  empty_lists.InitWithNullFeatureAndFieldTrialLists();
  base::FieldTrialList field_trial_list;

  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->InitFromCommandLine("", "ClientIdBackupCorrectnessFix");

  variations::MockEntropyProviders entropy_providers(
      {.low_entropy = variations::kAlwaysUseFirstGroup});
  CreateClientIdBackupCorrectnessFixFieldTrialForTesting(
      *feature_list, entropy_providers.default_entropy(),
      version_info::Channel::CANARY);

  EXPECT_FALSE(base::FieldTrialList::TrialExists(
      kClientIdBackupCorrectnessFixTrialName));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(features::kClientIdBackupCorrectnessFix));
}

TEST(ClientIdBackupCorrectnessFixFieldTrialTest, IdempotentOnRepeatedCalls) {
  base::test::ScopedFeatureList empty_lists;
  empty_lists.InitWithNullFeatureAndFieldTrialLists();
  base::FieldTrialList field_trial_list;

  auto feature_list = std::make_unique<base::FeatureList>();
  variations::MockEntropyProviders enabled_entropy(
      {.low_entropy = variations::kAlwaysUseFirstGroup});
  variations::MockEntropyProviders disabled_entropy(
      {.low_entropy = variations::kAlwaysUseLastGroup});

  CreateClientIdBackupCorrectnessFixFieldTrialForTesting(
      *feature_list, enabled_entropy.default_entropy(),
      version_info::Channel::CANARY);
  // Second call should be a no-op and not crash or overwrite the first choice.
  CreateClientIdBackupCorrectnessFixFieldTrialForTesting(
      *feature_list, disabled_entropy.default_entropy(),
      version_info::Channel::CANARY);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(features::kClientIdBackupCorrectnessFix));
}

}  // namespace
}  // namespace metrics
