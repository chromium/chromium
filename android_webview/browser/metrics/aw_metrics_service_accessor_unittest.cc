// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_metrics_service_accessor.h"

#include <vector>

#include "android_webview/browser/metrics/aw_metrics_test_utils.h"
#include "base/test/scoped_feature_list.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_associated_data.h"

namespace android_webview {

namespace {

bool HasTrial(const std::vector<variations::ActiveGroupId>& trials,
              std::string_view trial_name,
              std::string_view group_name) {
  variations::ActiveGroupId id =
      variations::MakeActiveGroupId(trial_name, group_name);
  for (const auto& trial : trials) {
    if (trial.name == id.name && trial.group == id.group) {
      return true;
    }
  }
  return false;
}

}  // namespace

class AwMetricsServiceAccessorTest : public AwMetricsTestBase {
 protected:
  variations::SyntheticTrialRegistry* GetRegistry() {
    return AwMetricsServiceClient::GetInstance()
        ->GetMetricsService()
        ->GetSyntheticTrialRegistry();
  }
};

TEST_F(AwMetricsServiceAccessorTest,
       RegisterExternalExperimentUpdatesCorrectly) {
  // Setup allowlist:
  // 100 -> StudyA,Group1
  // 101 -> StudyA,Group2
  // 200 -> StudyB,Group1
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["100"] = "StudyA,Group1";
  params["101"] = "StudyA,Group2";
  params["200"] = "StudyB,Group1";
  feature_list.InitAndEnableFeatureWithParameters(
      variations::internal::kExternalExperimentAllowlist, params);

  // 1. Register ID 100
  AwMetricsServiceAccessor::RegisterExternalExperiment({100});

  std::vector<variations::ActiveGroupId> trials;
  trials = GetRegistry()->GetCurrentSyntheticFieldTrialsForTest();
  EXPECT_TRUE(HasTrial(trials, "StudyA", "Group1"));

  // 2. Register ID 200 (Replaces StudyA if it were the same study, but here it
  // adds StudyB) Note: Since we use kOverrideExistingIds in the accessor, this
  // replaces the whole set.
  AwMetricsServiceAccessor::RegisterExternalExperiment({200});

  trials = GetRegistry()->GetCurrentSyntheticFieldTrialsForTest();
  EXPECT_FALSE(HasTrial(trials, "StudyA", "Group1"));
  EXPECT_TRUE(HasTrial(trials, "StudyB", "Group1"));

  // 3. Register multiple IDs
  AwMetricsServiceAccessor::RegisterExternalExperiment({100, 200});
  trials = GetRegistry()->GetCurrentSyntheticFieldTrialsForTest();
  EXPECT_TRUE(HasTrial(trials, "StudyA", "Group1"));
  EXPECT_TRUE(HasTrial(trials, "StudyB", "Group1"));

  // 4. Update StudyA to Group2
  AwMetricsServiceAccessor::RegisterExternalExperiment({101, 200});

  trials = GetRegistry()->GetCurrentSyntheticFieldTrialsForTest();
  EXPECT_FALSE(HasTrial(trials, "StudyA", "Group1"));
  EXPECT_TRUE(HasTrial(trials, "StudyA", "Group2"));
  EXPECT_TRUE(HasTrial(trials, "StudyB", "Group1"));

  // 5. Unregister all (empty list)
  AwMetricsServiceAccessor::RegisterExternalExperiment({});

  trials = GetRegistry()->GetCurrentSyntheticFieldTrialsForTest();
  EXPECT_FALSE(HasTrial(trials, "StudyA", "Group2"));
  EXPECT_FALSE(HasTrial(trials, "StudyB", "Group1"));
}

TEST_F(AwMetricsServiceAccessorTest,
       RegisterExternalExperimentOrderingAgnostic) {
  // Setup allowlist:
  // 100 -> StudyA,Group1
  // 101 -> StudyA,Group2
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["100"] = "StudyA,Group1";
  params["101"] = "StudyA,Group2";
  feature_list.InitAndEnableFeatureWithParameters(
      variations::internal::kExternalExperimentAllowlist, params);

  // 1. Register {100, 101}
  AwMetricsServiceAccessor::RegisterExternalExperiment({100, 101});

  std::vector<variations::ActiveGroupId> trials;
  trials = GetRegistry()->GetCurrentSyntheticFieldTrialsForTest();
  EXPECT_TRUE(HasTrial(trials, "StudyA", "Group1"));
  EXPECT_TRUE(HasTrial(trials, "StudyA", "Group2"));

  // 2. Register {101, 100} (Different order)
  // This should be a no-op and NOT crash or change anything.
  // We can't easily verify "no-op" without a mock, but we verify it doesn't
  // remove the trials.
  AwMetricsServiceAccessor::RegisterExternalExperiment({101, 100});

  trials = GetRegistry()->GetCurrentSyntheticFieldTrialsForTest();
  EXPECT_TRUE(HasTrial(trials, "StudyA", "Group1"));
  EXPECT_TRUE(HasTrial(trials, "StudyA", "Group2"));
}

}  // namespace android_webview
