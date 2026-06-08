// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cros_pre_choice_metrics_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/metrics/metrics_reporting_level.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
class CrOSPreChoiceMetricsManagerTest : public InProcessBrowserTest {
 public:
  CrOSPreChoiceMetricsManagerTest() {
    feature_list_.InitAndEnableFeature(ash::features::kOobePreConsentMetrics);

    // Make sure that the pref is used to check the consent during tests.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
  }

  ~CrOSPreChoiceMetricsManagerTest() override {
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        false);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void WaitOnConsentToPropagate() {
    base::RunLoop run_loop;
    GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrOSPreChoiceMetricsManagerTest,
                       EnablePreChoiceMetrics) {
  // Check that the default instance is in the expected state.
  CrOSPreChoiceMetricsManager* manager = CrOSPreChoiceMetricsManager::Get();
  ASSERT_NE(manager, nullptr);
  EXPECT_FALSE(manager->is_enabled_for_testing());

  manager->Enable();
  EXPECT_TRUE(manager->is_enabled_for_testing());
  WaitOnConsentToPropagate();

  // Metrics reporting and recording should be enabled.
  EXPECT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  EXPECT_TRUE(g_browser_process->GetMetricsServicesManager()
                  ->IsMetricsReportingEnabled());
}

IN_PROC_BROWSER_TEST_F(CrOSPreChoiceMetricsManagerTest,
                       DisablePreChoiceMetrics) {
  CrOSPreChoiceMetricsManager* manager = CrOSPreChoiceMetricsManager::Get();
  ASSERT_NE(manager, nullptr);

  base::FilePath completed_path = temp_dir_.GetPath().Append("test_file");
  manager->SetCompletedPathForTesting(completed_path);

  EXPECT_FALSE(manager->is_enabled_for_testing());

  manager->Enable();
  EXPECT_TRUE(manager->is_enabled_for_testing());
  WaitOnConsentToPropagate();

  EXPECT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  // Disable the Pre-choice metrics.
  manager->Disable();
  EXPECT_FALSE(manager->is_enabled_for_testing());
  WaitOnConsentToPropagate();

  // The existing state of metrics is not changed when disabled.
  EXPECT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  EXPECT_TRUE(g_browser_process->GetMetricsServicesManager()
                  ->IsMetricsReportingEnabled());

  // Wait for the file to be written.
  base::RunLoop run_loop;
  manager->PostToIOTaskRunnerForTesting(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // The path should exist.
  base::RunLoop run_loop2;
  bool closure_ran = false;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::MayBlock(), base::BindLambdaForTesting([&]() {
        closure_ran = true;
        EXPECT_TRUE(base::PathExists(completed_path));
      }),
      run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_TRUE(closure_ran);
}

IN_PROC_BROWSER_TEST_F(CrOSPreChoiceMetricsManagerTest,
                       EnableSetsBasicLevelWithRestructure) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(
      metrics::prefs::kMetricsConsentRestructureFeatureState, true);
  local_state->SetBoolean(metrics::prefs::kMetricsReportingMigrationDone, true);
  metrics::MetricsReportingChoiceService::ClearCachedFeatureStateForTesting();
  ASSERT_TRUE(metrics::MetricsReportingChoiceService::
                  ShouldUseMetricsConsentRestructure(local_state));

  CrOSPreChoiceMetricsManager* manager = CrOSPreChoiceMetricsManager::Get();
  ASSERT_NE(manager, nullptr);

  manager->Enable();
  WaitOnConsentToPropagate();

  EXPECT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());
  EXPECT_EQ(static_cast<int>(metrics::MetricsReportingLevel::kBasic),
            local_state->GetInteger(metrics::prefs::kMetricsReportingLevel));
}

class OwnedDeviceCrOSPreChoiceMetricsManagerTest
    : public MixinBasedInProcessBrowserTest {
 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
};

IN_PROC_BROWSER_TEST_F(OwnedDeviceCrOSPreChoiceMetricsManagerTest,
                       DeviceOwned) {
  ASSERT_EQ(CrOSPreChoiceMetricsManager::Get(), nullptr);
}

}  // namespace metrics
