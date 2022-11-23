// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/consolidated_consent_field_trial.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/channel_info.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::consolidated_consent_field_trial {

using ::testing::Eq;

class ConsolidatedConsentFieldTrialTest : public testing::Test {
 protected:
  ConsolidatedConsentFieldTrialTest() {}

  void SetUp() override { RegisterLocalStatePrefs(pref_service_.registry()); }

  PrefService* local_state() { return &pref_service_; }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(ConsolidatedConsentFieldTrialTest,
       FieldTrialCreatesProperlyWithNoExistingPref) {
  base::FeatureList feature_list;

  // Assert that pref has not been assigned.
  EXPECT_THAT(local_state()->GetString(kTrialGroupPrefName),
              ::testing::IsEmpty());

  base::MockEntropyProvider entropy_provider(0.9);
  Create(entropy_provider, &feature_list, local_state());

  // Ensure that the correct state is preserved so on the next run, the trial
  // does not change.
  const std::string expected_pref_value =
      local_state()->GetString(kTrialGroupPrefName);

  // This check is here in case experiment is disabled.
  if (ShouldEnableTrial(chrome::GetChannel())) {
    EXPECT_TRUE(expected_pref_value == kEnabledGroup ||
                expected_pref_value == kDisabledGroup);

    auto* trial = base::FieldTrialList::Find(kTrialName);

    // Ensure that the trial group assigned is the same as the group stored in
    // the pref and has not been activated.
    EXPECT_EQ(expected_pref_value, trial->GetGroupNameWithoutActivation());
    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

    EXPECT_TRUE(feature_list.IsFeatureOverridden(
        features::kOobeConsolidatedConsent.name));
    EXPECT_TRUE(
        feature_list.IsFeatureOverridden(features::kPerUserMetrics.name));
  } else {
    EXPECT_THAT(base::FieldTrialList::Find(kTrialName), ::testing::IsNull());
  }
}

class ConsolidatedConsentFieldTrialExistingGroupTest
    : public ConsolidatedConsentFieldTrialTest,
      public ::testing::WithParamInterface<const char*> {};

TEST_P(ConsolidatedConsentFieldTrialExistingGroupTest,
       FieldTrialUsesLocalStateGroupIfExists) {
  base::FeatureList feature_list;
  const std::string group_name = GetParam();

  // assert that pref has not been assigned.
  EXPECT_THAT(local_state()->GetString(kTrialGroupPrefName), Eq(""));

  // Set pref to enabled group.
  local_state()->SetString(kTrialGroupPrefName, group_name);
  base::MockEntropyProvider entropy_provider(0.9);
  Create(entropy_provider, &feature_list, local_state());

  // This check is here in case experiment is disabled.
  if (ShouldEnableTrial(chrome::GetChannel())) {
    // Pref should not change.
    const std::string expected_pref_value =
        local_state()->GetString(kTrialGroupPrefName);
    EXPECT_EQ(expected_pref_value, group_name);

    auto* trial = base::FieldTrialList::Find(kTrialName);

    // Field trial should have the same value as the pref.
    EXPECT_EQ(group_name, trial->GetGroupNameWithoutActivation());

    EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));
    EXPECT_TRUE(feature_list.IsFeatureOverridden(
        features::kOobeConsolidatedConsent.name));
    EXPECT_TRUE(
        feature_list.IsFeatureOverridden(features::kPerUserMetrics.name));

    // TODO(jongahn): Figure out how to check feature state without
    // |feature_list| being initialized or how to initialize |feature_list|.
  } else {
    EXPECT_THAT(base::FieldTrialList::Find(kTrialName), ::testing::IsNull());
  }
}

INSTANTIATE_TEST_SUITE_P(FieldTrialUsesLocalStateGroupIfExists,
                         ConsolidatedConsentFieldTrialExistingGroupTest,
                         testing::ValuesIn({kDisabledGroup, kEnabledGroup}));

}  // namespace ash::consolidated_consent_field_trial
