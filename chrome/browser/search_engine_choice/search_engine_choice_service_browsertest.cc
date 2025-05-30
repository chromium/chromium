// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"

#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/cloned_install_detector.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/test/scoped_metrics_id_provider.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_utils.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using search_engines::GetChoiceCompletionMetadata;
using search_engines::SearchEngineChoiceScreenConditions;
using search_engines::SearchEngineChoiceService;
using search_engines::SearchEngineChoiceServiceFactory;

class SearchEngineChoiceServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry,
                                    switches::kDefaultListCountryOverride);
  }

  SearchEngineChoiceScreenConditions GetStaticConditions(
      Profile* profile,
      bool is_regular_profile = true) {
    SearchEngineChoiceService* search_engine_choice_service =
        SearchEngineChoiceServiceFactory::GetForProfile(profile);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);

    return search_engine_choice_service->GetStaticChoiceScreenConditions(
        CHECK_DEREF(g_browser_process->policy_service()), is_regular_profile,
        *template_url_service);
  }

  bool HasChoiceTimestamp(Profile* profile) {
    return profile->GetPrefs()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp);
  }

  base::AutoReset<bool> scoped_chrome_build_override_ =
      SearchEngineChoiceDialogServiceFactory::
          ScopedChromeBuildOverrideForTesting(
              /*force_chrome_build=*/true);
};

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceServiceBrowserTest,
                       StaticCondition_IsEligible) {
  EXPECT_EQ(GetStaticConditions(browser()->profile()),
            SearchEngineChoiceScreenConditions::kEligible);
}

enum class FeatureState {
  kDisabled,
  kEnabledJustInTime,
  kEnabledRetroactive,
};

struct RunExpectations {
  // Whether the dialog service is expected to have been created
  bool has_dialog_service;

  // Whether the completion timestamp is still present
  bool has_choice_timestamp;

  // Whether the choice is considered completed after the services are done
  // initializing and processed the prefs.
  bool has_completed_choice;
};

struct RestoreTestParam {
  std::string test_name;
  FeatureState feature_state;
  const RunExpectations run_0 = {
      // Run 0 expectations are hardcoded since the feature has no effect on it.
      .has_dialog_service = true,
      .has_choice_timestamp = true,
      .has_completed_choice = true,
  };
  RunExpectations run_1_expectations;
  RunExpectations run_2_expectations;
};

class SearchEngineChoiceServiceRestoreBrowserTest
    : public SearchEngineChoiceServiceBrowserTest,
      public testing::WithParamInterface<RestoreTestParam> {
 public:
  void SetUp() override {
    scoped_machine_id_provider_.machine_id =
        GetTestPreCount() == 2 ? "pre_restore_id" : "post_restore_id";

    switch (GetParam().feature_state) {
      case FeatureState::kDisabled:
        feature_list_.InitAndDisableFeature(
            switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection);
        break;
      case FeatureState::kEnabledJustInTime:
        feature_list_.InitAndEnableFeature(
            switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection);
        break;
      case FeatureState::kEnabledRetroactive:
        feature_list_.InitAndEnableFeatureWithParameters(
            switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection,
            {{"is_retroactive", "true"}});
        break;
    }

    SearchEngineChoiceServiceBrowserTest::SetUp();
  }

  RunExpectations GetRunExpectations() {
    if (GetTestPreCount() == 2) {
      return GetParam().run_0;
    }
    if (GetTestPreCount() == 1) {
      return GetParam().run_1_expectations;
    }
    if (GetTestPreCount() == 0) {
      return GetParam().run_2_expectations;
    }
    NOTREACHED() << "Unexpected TestPreCount: " << GetTestPreCount();
  }

  metrics::ScopedMachineIdProvider scoped_machine_id_provider_;
  base::test::ScopedFeatureList feature_list_;
};

const RestoreTestParam kTestParams[] = {
    {.test_name = "FeatureDisabled",
     .feature_state = FeatureState::kDisabled,
     .run_1_expectations = {.has_dialog_service = false,
                            .has_choice_timestamp = true,
                            .has_completed_choice = true},
     .run_2_expectations = {.has_dialog_service = false,
                            .has_choice_timestamp = true,
                            .has_completed_choice = true}},
    {.test_name = "FeatureEnabledJustInTime",
     .feature_state = FeatureState::kEnabledJustInTime,
     // Run 1: Ideally the choice should not be considered completed anymore,
     // but it technically infeasible on Desktop platforms.
     // The clone detection happens on a low-priority background task, and it
     // completes after we are done checking the choice screen eligibility
     // status and declined initializing the dialog service.
     .run_1_expectations = {.has_dialog_service = false,
                            .has_choice_timestamp = true,
                            .has_completed_choice = true},
     // Run 2:  Since the choice was not invalidated in the session where the
     // clone was detected, for the "JustInTime" mode, we don't wipe the choice
     // timestamp later either. this makes this mode very limited on Desktop.
     .run_2_expectations = {.has_dialog_service = false,
                            .has_choice_timestamp = true,
                            .has_completed_choice = true}},
    {.test_name = "FeatureEnabledRetroactive",
     .feature_state = FeatureState::kEnabledRetroactive,
     // Run 1: Same as the "JustInTime" version.
     .run_1_expectations = {.has_dialog_service = false,
                            .has_choice_timestamp = true,
                            .has_completed_choice = true},
     // Run 2: We are able to wipe the choice timestamp and make remake the
     // profile eligible to get the choice dialog.
     .run_2_expectations = {.has_dialog_service = true,
                            .has_choice_timestamp = false,
                            .has_completed_choice = false}},
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SearchEngineChoiceServiceRestoreBrowserTest,
    testing::ValuesIn(kTestParams),
    [](const ::testing::TestParamInfo<RestoreTestParam>& info) {
      return info.param.test_name;
    });

// Run 0, where we mark the profile as having made a search engine choice.
IN_PROC_BROWSER_TEST_P(SearchEngineChoiceServiceRestoreBrowserTest,
                       PRE_PRE_StaticConditions) {
  Profile* profile = browser()->profile();

  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile);
  ASSERT_FALSE(search_engine_choice_service->GetClientForTesting()
                   .IsDeviceRestoreDetectedInCurrentSession());

  ASSERT_EQ(GetStaticConditions(profile),
            SearchEngineChoiceScreenConditions::kEligible);

  ASSERT_EQ(GetRunExpectations().has_dialog_service,
            !!SearchEngineChoiceDialogServiceFactory::GetForProfile(profile));

  // Marking the choice as made.
  ASSERT_FALSE(HasChoiceTimestamp(profile));
  search_engine_choice_service->RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      TemplateURLServiceFactory::GetForProfile(profile));
  ASSERT_EQ(GetRunExpectations().has_choice_timestamp,
            HasChoiceTimestamp(profile));

  ASSERT_EQ(GetRunExpectations().has_completed_choice,
            GetChoiceCompletionMetadata(*profile->GetPrefs()).has_value());
  ASSERT_EQ(GetStaticConditions(profile),
            SearchEngineChoiceScreenConditions::kAlreadyCompleted);
}

// Run 1, the first one happening after a device restore.
IN_PROC_BROWSER_TEST_P(SearchEngineChoiceServiceRestoreBrowserTest,
                       PRE_StaticConditions) {
  // The current session has the detection but not the ID reset.
  ASSERT_TRUE(g_browser_process->GetMetricsServicesManager()
                  ->GetClonedInstallDetectorForTesting()
                  ->ClonedInstallDetectedInCurrentSession());
  ASSERT_EQ(metrics::ClonedInstallDetector::ReadClonedInstallInfo(
                g_browser_process->local_state())
                .reset_count,
            0);

  Profile* profile = browser()->profile();
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(search_engine_choice_service->GetClientForTesting()
                  .IsDeviceRestoreDetectedInCurrentSession());
  auto choice_completion_metadata =
      GetChoiceCompletionMetadata(*profile->GetPrefs());

  EXPECT_EQ(GetRunExpectations().has_dialog_service,
            !!SearchEngineChoiceDialogServiceFactory::GetForProfile(profile));
  EXPECT_EQ(GetRunExpectations().has_choice_timestamp,
            HasChoiceTimestamp(profile));

  // The choice has not been wiped, but we know that it predates restore.
  EXPECT_EQ(GetRunExpectations().has_completed_choice,
            choice_completion_metadata.has_value());
  if (GetRunExpectations().has_completed_choice) {
    EXPECT_TRUE(search_engine_choice_service->GetClientForTesting()
                    .DoesChoicePredateDeviceRestore(
                        choice_completion_metadata.value()));
  }

  EXPECT_EQ(GetStaticConditions(profile),
            SearchEngineChoiceScreenConditions::kAlreadyCompleted);
}

// Run 2, where the metrics ID gets reset following the clone detection.
IN_PROC_BROWSER_TEST_P(SearchEngineChoiceServiceRestoreBrowserTest,
                       StaticConditions) {
  // The clone was detected in the previous session, but we reset the ID
  // starting in this one.
  ASSERT_FALSE(g_browser_process->GetMetricsServicesManager()
                   ->GetClonedInstallDetectorForTesting()
                   ->ClonedInstallDetectedInCurrentSession());
  ASSERT_EQ(metrics::ClonedInstallDetector::ReadClonedInstallInfo(
                g_browser_process->local_state())
                .reset_count,
            1);

  Profile* profile = browser()->profile();
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile);
  ASSERT_FALSE(search_engine_choice_service->GetClientForTesting()
                   .IsDeviceRestoreDetectedInCurrentSession());

  auto choice_completion_metadata =
      GetChoiceCompletionMetadata(*profile->GetPrefs());

  EXPECT_EQ(GetRunExpectations().has_dialog_service,
            !!SearchEngineChoiceDialogServiceFactory::GetForProfile(profile));
  EXPECT_EQ(GetRunExpectations().has_choice_timestamp,
            HasChoiceTimestamp(profile));

  EXPECT_EQ(GetRunExpectations().has_completed_choice,
            choice_completion_metadata.has_value());
  if (GetRunExpectations().has_completed_choice) {
    // The choice has not been wiped, but we know that it predates restore.
    EXPECT_TRUE(search_engine_choice_service->GetClientForTesting()
                    .DoesChoicePredateDeviceRestore(
                        choice_completion_metadata.value()));
  }

  EXPECT_EQ(GetStaticConditions(profile),
            GetRunExpectations().has_completed_choice
                ? SearchEngineChoiceScreenConditions::kAlreadyCompleted
                : SearchEngineChoiceScreenConditions::kEligible);
}

}  // namespace
