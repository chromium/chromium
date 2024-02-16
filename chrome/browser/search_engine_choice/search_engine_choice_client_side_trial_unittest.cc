// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_client_side_trial.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_metrics_service_for_synthetic_trials.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct SearchEngineChoiceFieldTrialTestParams {
  double entropy_value = 0.0;
  version_info::Channel channel = version_info::Channel::UNKNOWN;

  bool expect_study_enabled = false;
  bool expect_feature_enabled = false;
};

}  // namespace

class SearchEngineChoiceClientSideTrialTest
    : public testing::Test,
      public testing::WithParamInterface<
          SearchEngineChoiceFieldTrialTestParams> {
 public:
  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

  TestingPrefServiceSimple* local_state() { return testing_local_state_.Get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_;

  ScopedTestingLocalState testing_local_state_{
      TestingBrowserProcess::GetGlobal()};

  // Needed for synthetic trial checks to work.
  ScopedMetricsServiceForSyntheticTrials testing_metrics_service_{
      TestingBrowserProcess::GetGlobal()};
};

TEST_P(SearchEngineChoiceClientSideTrialTest, SetUpIfNeeded) {
  {
    auto scoped_channel_override = SearchEngineChoiceClientSideTrial::
        CreateScopedChannelOverrideForTesting(GetParam().channel);
    base::MockEntropyProvider low_entropy_provider{GetParam().entropy_value};
    auto feature_list = std::make_unique<base::FeatureList>();

    SearchEngineChoiceClientSideTrial::SetUpIfNeeded(
        low_entropy_provider, feature_list.get(), local_state());

    // Substitute the existing feature list with the one with field trial
    // configurations we are testing, so we can check the assertions.
    scoped_feature_list().InitWithFeatureList(std::move(feature_list));
  }

  EXPECT_EQ(GetParam().expect_feature_enabled,
            base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger));

  // Using explicit checks per branch here because the value of this property
  // depends not only on the study state set in the test, but also on the
  // hardcoded default value, which might be subject to cherry picks on branch.
  if (GetParam().expect_feature_enabled) {
    // Client-side study config explicitly sets it to true.
    EXPECT_TRUE(
        switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.Get());
  } else {
    // Default value of the flag, independent from the feature state.
    EXPECT_TRUE(
        switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.Get());
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive("WaffleStudy"));

  std::string expected_group_name =
      GetParam().expect_study_enabled
          ? GetParam().expect_feature_enabled
                ? "ClientSideEnabledForTaggedProfiles"
                : "ClientSideDisabled"
          : "Default";

  EXPECT_EQ(local_state()->GetString(prefs::kSearchEnginesStudyGroup),
            expected_group_name);
#else
  // No group is assigned on other platforms and nothing is added to prefs.
  EXPECT_TRUE(
      local_state()->GetString(prefs::kSearchEnginesStudyGroup).empty());
#endif
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SearchEngineChoiceClientSideTrialTest,
    testing::Values(
        // `entropy_value` makes the group be assigned according to the
        // specified weight of each group and the order in which they are
        // declared. So for a split at 33% enabled, 33% disabled, 33% default
        // a .4 entropy value should select the "disabled" group.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
        // Today, the feature is enabled by default, never enroll clients.
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.01,
            .channel = version_info::Channel::BETA,
            .expect_study_enabled = false,
            .expect_feature_enabled = false},
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.01,
            .channel = version_info::Channel::STABLE,
            .expect_study_enabled = false,
            .expect_feature_enabled = false}
#elif BUILDFLAG(IS_CHROMEOS)
        // Did not have a client-side field trial, so it's not bundled with the
        // group above, but it's being enabled on a different schedule than the
        // group below.
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.01,
            .channel = version_info::Channel::BETA,
            .expect_study_enabled = false,
            .expect_feature_enabled = false},
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.01,
            .channel = version_info::Channel::STABLE,
            .expect_study_enabled = false,
            .expect_feature_enabled = false}
#else
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.01,
            .channel = version_info::Channel::BETA,
            // On other platforms we never enroll clients.
            .expect_study_enabled = false,
            .expect_feature_enabled = false},
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.01,
            .channel = version_info::Channel::STABLE,
            // On other platforms we never enroll clients.
            .expect_study_enabled = false,
            .expect_feature_enabled = false}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
        ),

    [](const ::testing::TestParamInfo<SearchEngineChoiceFieldTrialTestParams>&
           params) {
      return base::StringPrintf(
          "%02.0fpctEntropy%s", params.param.entropy_value * 100,
          version_info::GetChannelString(params.param.channel).data());
    });

TEST_F(SearchEngineChoiceClientSideTrialTest,
       SetUpIfNeeded_SkipsIfFeatureOverridden) {
  {
    base::MockEntropyProvider low_entropy_provider{0.01};
    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->RegisterExtraFeatureOverrides(
        {{std::cref(switches::kSearchEngineChoiceTrigger),
          base::FeatureList::OVERRIDE_ENABLE_FEATURE}});

    SearchEngineChoiceClientSideTrial::SetUpIfNeeded(
        low_entropy_provider, feature_list.get(), local_state());

    // Substitute the existing feature list with the one with field trial
    // configurations we are testing, so we can check the assertions.
    scoped_feature_list().InitWithFeatureList(std::move(feature_list));
  }

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("WaffleStudy"));
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kSearchEnginesStudyGroup));
}

TEST_F(SearchEngineChoiceClientSideTrialTest,
       RegisterSyntheticTrials_ReadsPref) {
  const char kStudyTestGroupName1[] = "group_name_1";
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kSearchEnginesStudyGroup));
  EXPECT_FALSE(variations::HasSyntheticTrial(
      SearchEngineChoiceClientSideTrial::kSyntheticTrialName));

  // `RegisterSyntheticTrials()` no-ops without some specific pref.
  SearchEngineChoiceClientSideTrial::RegisterSyntheticTrials();
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kSearchEnginesStudyGroup));
  EXPECT_FALSE(variations::HasSyntheticTrial(
      SearchEngineChoiceClientSideTrial::kSyntheticTrialName));

  // With the pref, it will log it as synthetic trial group.
  local_state()->SetString(prefs::kSearchEnginesStudyGroup,
                           kStudyTestGroupName1);
  SearchEngineChoiceClientSideTrial::RegisterSyntheticTrials();
  EXPECT_TRUE(variations::HasSyntheticTrial(
      SearchEngineChoiceClientSideTrial::kSyntheticTrialName));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      SearchEngineChoiceClientSideTrial::kSyntheticTrialName,
      kStudyTestGroupName1));
}
