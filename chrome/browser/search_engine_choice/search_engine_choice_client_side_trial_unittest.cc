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
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "components/version_info/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
#error "Unsupported platform"
#endif

namespace {

// TODO(b/313407392): Move the class to some utils file.
class ScopedTestingMetricsService {
 public:
  // Sets up a `metrics::MetricsService` instance and makes it available in its
  // scope via `testing_browser_process->metrics_service()`.
  //
  // This service only supports feature related to the usage of synthetic field
  // trials.
  //
  // Requires:
  // - the local state prefs to be usable from `testing_browser_process`
  // - a task runner to be available (see //docs/threading_and_tasks_testing.md)
  explicit ScopedTestingMetricsService(
      TestingBrowserProcess* testing_browser_process)
      : browser_process_(testing_browser_process) {
    CHECK(browser_process_);

    auto* local_state = browser_process_->local_state();
    CHECK(local_state)
        << "Error: local state prefs are required. In a unit test, this can be "
           "set up using base::test::ScopedTestingLocalState.";

    // The `SyntheticTrialsActiveGroupIdProvider` needs to be notified of
    // changes from the registry for them to be used through the variations API.
    synthetic_trial_registry_observation_.Observe(&synthetic_trial_registry_);

    metrics_service_client_.set_synthetic_trial_registry(
        &synthetic_trial_registry_);

    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state, &enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath());

    // Needs to be set up, will be updated at each synthetic trial change.
    variations::InitCrashKeys();

    // Required by `MetricsService` to record UserActions. We don't rely on
    // these here, since we never make it start recording metrics, but the task
    // runner is still required during the shutdown sequence.
    base::SetRecordActionTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());

    metrics_service_ = std::make_unique<metrics::MetricsService>(
        metrics_state_manager_.get(), &metrics_service_client_, local_state);

    browser_process_->SetMetricsService(metrics_service_.get());
  }

  ~ScopedTestingMetricsService() {
    // The scope is closing, undo the set up that was done in the constructor:
    // `MetricsService` and other necessary parts like crash keys.
    browser_process_->SetMetricsService(nullptr);
    variations::ClearCrashKeysInstanceForTesting();

    // Note: Clears all the synthetic trials, not juste the ones registered
    // during the lifetime of this object.
    variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()
        ->ResetForTesting();
  }

  metrics::MetricsService* Get() { return metrics_service_.get(); }

 private:
  raw_ptr<TestingBrowserProcess> browser_process_ = nullptr;

  metrics::TestEnabledStateProvider enabled_state_provider_{/*consent=*/true,
                                                            /*enabled=*/true};

  variations::SyntheticTrialRegistry synthetic_trial_registry_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_registry_observation_{
          variations::SyntheticTrialsActiveGroupIdProvider::GetInstance()};

  metrics::TestMetricsServiceClient metrics_service_client_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  std::unique_ptr<metrics::MetricsService> metrics_service_;
};

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
  ScopedTestingMetricsService testing_metrics_service_{
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
  EXPECT_EQ(GetParam().expect_feature_enabled,
            switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.Get());
  EXPECT_EQ(GetParam().expect_feature_enabled,
            base::FeatureList::IsEnabled(switches::kSearchEngineChoice));
  EXPECT_EQ(GetParam().expect_feature_enabled,
            base::FeatureList::IsEnabled(switches::kSearchEngineChoiceFre));

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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
        // `entropy_value` makes the group be assigned according to the
        // specified weight of each group and the order in which they are
        // declared. So for a split at 33% enabled, 33% disabled, 33% default
        // a .4 entropy value should select the "disabled" group.
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.01,
            .channel = version_info::Channel::BETA,
            // In the 50% treatment group
            .expect_study_enabled = true,
            .expect_feature_enabled = true},
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.6,
            .channel = version_info::Channel::BETA,
            // In the 50% control group
            .expect_study_enabled = true,
            .expect_feature_enabled = false},
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.0001,
            .channel = version_info::Channel::STABLE,
            // In the .5% treatment group
            .expect_study_enabled = true,
            .expect_feature_enabled = true},
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.009,
            .channel = version_info::Channel::STABLE,
            // In the .5% control group
            .expect_study_enabled = true,
            .expect_feature_enabled = false},
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.99,
            .channel = version_info::Channel::STABLE,
            // Not in the study (99%)
            .expect_study_enabled = false,
            .expect_feature_enabled = false}
#else
        SearchEngineChoiceFieldTrialTestParams{
            .entropy_value = 0.01,
            .channel = version_info::Channel::BETA,
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
        {{std::cref(switches::kSearchEngineChoice),
          base::FeatureList::OVERRIDE_ENABLE_FEATURE}});

    SearchEngineChoiceClientSideTrial::SetUpIfNeeded(
        low_entropy_provider, feature_list.get(), local_state());

    // Substitute the existing feature list with the one with field trial
    // configurations we are testing, so we can check the assertions.
    scoped_feature_list().InitWithFeatureList(std::move(feature_list));
  }

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive("WaffleStudy"));

  EXPECT_FALSE(
      base::FeatureList::IsEnabled(switches::kSearchEngineChoiceTrigger));
  EXPECT_TRUE(base::FeatureList::IsEnabled(switches::kSearchEngineChoice));

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
