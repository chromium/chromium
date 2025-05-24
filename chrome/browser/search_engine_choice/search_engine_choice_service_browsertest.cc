// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"

#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
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

struct RestoreTestParam {
  std::string test_name;
  FeatureState feature_state;
  SearchEngineChoiceScreenConditions run_1_expected_condition;
  SearchEngineChoiceScreenConditions run_2_expected_condition;
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

  metrics::ScopedMachineIdProvider scoped_machine_id_provider_;
  base::test::ScopedFeatureList feature_list_;
};

const RestoreTestParam kTestParams[] = {
    {.test_name = "FeatureDisabled",
     .feature_state = FeatureState::kDisabled,
     .run_1_expected_condition =
         SearchEngineChoiceScreenConditions::kAlreadyCompleted,
     .run_2_expected_condition =
         SearchEngineChoiceScreenConditions::kAlreadyCompleted},
    {.test_name = "FeatureEnabledJustInTime",
     .feature_state = FeatureState::kEnabledJustInTime,
     // Ideally `kEligible`, but technically infeasible on Desktop platforms.
     // The clone detection happens on a low-priority background task, and it
     // completes after we are done checking the choice screen eligibility
     // status.
     .run_1_expected_condition =
         SearchEngineChoiceScreenConditions::kAlreadyCompleted,
     // Since the choice was not invalidated in the session where the clone was
     // detected, for the "JustInTime" mode, we don't wipe it later either.
     .run_2_expected_condition =
         SearchEngineChoiceScreenConditions::kAlreadyCompleted},
    {.test_name = "FeatureEnabledRetroactive",
     .feature_state = FeatureState::kEnabledRetroactive,
     // Ideally `kEligible`, but just like the JustInTime version, we detect
     // the clone too late. The invalidation will be deferred to the next
     // session.
     .run_1_expected_condition =
         SearchEngineChoiceScreenConditions::kAlreadyCompleted,
     .run_2_expected_condition = SearchEngineChoiceScreenConditions::kEligible},
};

INSTANTIATE_TEST_SUITE_P(
    ,
    SearchEngineChoiceServiceRestoreBrowserTest,
    testing::ValuesIn(kTestParams),
    [](const ::testing::TestParamInfo<RestoreTestParam>& info) {
      return info.param.test_name;
    });

IN_PROC_BROWSER_TEST_P(SearchEngineChoiceServiceRestoreBrowserTest,
                       PRE_PRE_StaticConditions) {
  Profile* profile = browser()->profile();
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile);

  ASSERT_FALSE(GetChoiceCompletionMetadata(*profile->GetPrefs()).has_value());
  ASSERT_FALSE(search_engine_choice_service->GetClientForTesting()
                   .IsDeviceRestoreDetectedInCurrentSession());

  ASSERT_EQ(GetStaticConditions(profile),
            SearchEngineChoiceScreenConditions::kEligible);

  search_engine_choice_service->RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      TemplateURLServiceFactory::GetForProfile(profile));

  ASSERT_EQ(GetStaticConditions(profile),
            SearchEngineChoiceScreenConditions::kAlreadyCompleted);
}

IN_PROC_BROWSER_TEST_P(SearchEngineChoiceServiceRestoreBrowserTest,
                       PRE_StaticConditions) {
  auto* detector = g_browser_process->GetMetricsServicesManager()
                       ->GetClonedInstallDetectorForTesting();
  metrics::ClonedInstallInfo cloned_install_info =
      metrics::ClonedInstallDetector::ReadClonedInstallInfo(
          g_browser_process->local_state());

  // The current session has the detection but not the ID reset.
  ASSERT_TRUE(detector->ClonedInstallDetectedInCurrentSession());
  EXPECT_EQ(cloned_install_info.reset_count, 0);

  Profile* profile = browser()->profile();
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile);

  // The choice has not been wiped, but we know that it predates restore.
  auto choice_completion_metata =
      GetChoiceCompletionMetadata(*profile->GetPrefs());
  EXPECT_TRUE(choice_completion_metata.has_value());
  EXPECT_TRUE(
      search_engine_choice_service->GetClientForTesting()
          .DoesChoicePredateDeviceRestore(choice_completion_metata.value()));

  EXPECT_EQ(GetStaticConditions(profile), GetParam().run_1_expected_condition);
}

IN_PROC_BROWSER_TEST_P(SearchEngineChoiceServiceRestoreBrowserTest,
                       StaticConditions) {
  auto* detector = g_browser_process->GetMetricsServicesManager()
                       ->GetClonedInstallDetectorForTesting();
  metrics::ClonedInstallInfo cloned_install_info =
      metrics::ClonedInstallDetector::ReadClonedInstallInfo(
          g_browser_process->local_state());

  // The clone was detected in the previous session, but we reset the ID
  // starting in this one.
  ASSERT_FALSE(detector->ClonedInstallDetectedInCurrentSession());
  ASSERT_EQ(cloned_install_info.reset_count, 1);

  Profile* profile = browser()->profile();
  SearchEngineChoiceService* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(profile);

  auto choice_completion_metata =
      GetChoiceCompletionMetadata(*profile->GetPrefs());
  if (GetParam().run_2_expected_condition ==
      search_engines::SearchEngineChoiceScreenConditions::kAlreadyCompleted) {
    // The choice has not been wiped, but we know that it predates restore.
    EXPECT_TRUE(choice_completion_metata.has_value());
    EXPECT_TRUE(
        search_engine_choice_service->GetClientForTesting()
            .DoesChoicePredateDeviceRestore(choice_completion_metata.value()));
  } else {
    // The choice should have been wiped when the service is created.
    EXPECT_FALSE(choice_completion_metata.has_value());
  }

  // This is the second run after restore, it didn't happen in the current
  // session.
  EXPECT_FALSE(search_engine_choice_service->GetClientForTesting()
                   .IsDeviceRestoreDetectedInCurrentSession());

  EXPECT_EQ(GetStaticConditions(browser()->profile()),
            GetParam().run_2_expected_condition);
}

}  // namespace
