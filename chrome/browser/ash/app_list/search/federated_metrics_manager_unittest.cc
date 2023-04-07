// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/federated_metrics_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/system/federated/federated_service_controller.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chrome/browser/ash/app_list/app_list_notifier_impl.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/test/search_metrics_test_util.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chromeos/ash/components/dbus/federated/federated_client.h"
#include "chromeos/ash/services/federated/public/cpp/fake_service_connection.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using ash::FederatedClient;
using ash::federated::FakeServiceConnectionImpl;
using ash::federated::ScopedFakeServiceConnectionForTest;
using ash::federated::ServiceConnection;
using federated::FederatedMetricsManager;
using testing::HasSubstr;

class TestFederatedServiceController
    : public ash::federated::FederatedServiceController {
 public:
  TestFederatedServiceController() = default;
  TestFederatedServiceController(const TestFederatedServiceController&) =
      delete;
  TestFederatedServiceController& operator=(
      const TestFederatedServiceController&) = delete;

  // ash::federated::FederatedServiceController:
  bool IsServiceAvailable() const override { return true; }
};

// Parameterized by feature kLauncherQueryFederatedAnalyticsPHH.
class FederatedMetricsManagerTest : public testing::Test,
                                    public ::testing::WithParamInterface<bool> {
 public:
  FederatedMetricsManagerTest()
      : scoped_fake_for_test_(&fake_service_connection_),
        app_list_notifier_(&app_list_controller_) {
    std::vector<base::test::FeatureRef> enabled_features = {
        ash::features::kFederatedService};
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetParam()) {
      enabled_features.push_back(
          search_features::kLauncherQueryFederatedAnalyticsPHH);
    } else {
      disabled_features.push_back(
          search_features::kLauncherQueryFederatedAnalyticsPHH);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  FederatedMetricsManagerTest(const FederatedMetricsManagerTest&) = delete;
  FederatedMetricsManagerTest& operator=(const FederatedMetricsManagerTest&) =
      delete;

  // testing::Test:
  void SetUp() override {
    // Set up federated service connection.
    FederatedClient::InitializeFake();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override { FederatedClient::Shutdown(); }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  void SetChromeMetricsEnabled(bool value) {
    chrome_metrics_enabled_ = value;
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &chrome_metrics_enabled_);
  }

  void InitFederatedMetricsManager() {
    metrics_manager_ = std::make_unique<FederatedMetricsManager>(
        &app_list_notifier_, &federated_service_controller_);
  }

  void ExpectNoFederatedLogs() {
    const std::string histograms =
        histogram_tester()->GetAllHistogramsRecorded();
    EXPECT_THAT(
        histograms,
        Not(AnyOf(
            HasSubstr(app_list::federated::kHistogramInitStatus),
            HasSubstr(app_list::federated::kHistogramSearchSessionConclusion),
            HasSubstr(app_list::federated::kHistogramReportStatus))));
    // TODO(b/262611120): Check emptiness of federated service storage, once
    // this functionality is available.
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<FederatedMetricsManager> metrics_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  FakeServiceConnectionImpl fake_service_connection_;
  ScopedFakeServiceConnectionForTest scoped_fake_for_test_;

  ::test::TestAppListController app_list_controller_;
  AppListNotifierImpl app_list_notifier_;
  TestFederatedServiceController federated_service_controller_;

  bool chrome_metrics_enabled_;
};
INSTANTIATE_TEST_SUITE_P(LauncherQueryFA,
                         FederatedMetricsManagerTest,
                         testing::Bool());

TEST_P(FederatedMetricsManagerTest, ChromeMetricsConsentDisabled) {
  SetChromeMetricsEnabled(false);
  InitFederatedMetricsManager();

  // Simulate various user search activities.
  metrics_manager_->OnSearchSessionStarted();
  metrics_manager_->OnSearchSessionEnded(u"fake_query");

  metrics_manager_->OnSearchSessionStarted();
  std::vector<Result> shown_results;
  Result launched_result = CreateFakeResult(Type::EXTENSION_APP, "fake_id");
  std::u16string query = u"fake_query";
  metrics_manager_->OnSeen(Location::kAnswerCard, shown_results, query);
  metrics_manager_->OnLaunch(Location::kList, launched_result, shown_results,
                             query);
  metrics_manager_->OnSearchSessionEnded(u"fake_query");

  ExpectNoFederatedLogs();
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

TEST_P(FederatedMetricsManagerTest, Quit) {
  SetChromeMetricsEnabled(true);
  InitFederatedMetricsManager();

  metrics_manager_->OnSearchSessionStarted();
  // Search session ends without user taking other action (e.g. without
  // launching a result).
  metrics_manager_->OnSearchSessionEnded(u"fake_query");
  base::RunLoop().RunUntilIdle();

  const bool launcher_fa_enabled = GetParam();
  if (launcher_fa_enabled) {
    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramInitStatus,
        app_list::federated::FederatedMetricsManager::InitStatus::kOk, 1);

    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramSearchSessionConclusion,
        ash::SearchSessionConclusion::kQuit, 1);

    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramReportStatus,
        app_list::federated::FederatedMetricsManager::ReportStatus::kOk, 1);

    // TODO(b/262611120): Check contents of logged example, once this
    // functionality is available.
  } else {
    ExpectNoFederatedLogs();
  }
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

TEST_P(FederatedMetricsManagerTest, Launch) {
  SetChromeMetricsEnabled(true);
  InitFederatedMetricsManager();

  metrics_manager_->OnSearchSessionStarted();
  std::vector<Result> shown_results;
  Result launched_result = CreateFakeResult(Type::EXTENSION_APP, "fake_id");
  std::u16string query = u"fake_query";

  metrics_manager_->OnLaunch(Location::kList, launched_result, shown_results,
                             query);
  metrics_manager_->OnSearchSessionEnded(query);
  base::RunLoop().RunUntilIdle();

  const bool launcher_fa_enabled = GetParam();
  if (launcher_fa_enabled) {
    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramInitStatus,
        app_list::federated::FederatedMetricsManager::InitStatus::kOk, 1);

    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramSearchSessionConclusion,
        ash::SearchSessionConclusion::kLaunch, 1);

    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramReportStatus,
        app_list::federated::FederatedMetricsManager::ReportStatus::kOk, 1);
    // TODO(b/262611120): Check contents of logged example, once this
    // functionality is available.
  } else {
    ExpectNoFederatedLogs();
  }
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

TEST_P(FederatedMetricsManagerTest, AnswerCardSeen) {
  SetChromeMetricsEnabled(true);
  InitFederatedMetricsManager();

  metrics_manager_->OnSearchSessionStarted();
  std::vector<Result> shown_results;
  std::u16string query = u"fake_query";

  metrics_manager_->OnSeen(Location::kAnswerCard, shown_results, query);
  metrics_manager_->OnSearchSessionEnded(query);
  base::RunLoop().RunUntilIdle();

  const bool launcher_fa_enabled = GetParam();
  if (launcher_fa_enabled) {
    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramInitStatus,
        app_list::federated::FederatedMetricsManager::InitStatus::kOk, 1);

    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramSearchSessionConclusion,
        ash::SearchSessionConclusion::kAnswerCardSeen, 1);

    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramReportStatus,
        app_list::federated::FederatedMetricsManager::ReportStatus::kOk, 1);
    // TODO(b/262611120): Check contents of logged example, once this
    // functionality is available.
  } else {
    ExpectNoFederatedLogs();
  }
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

TEST_P(FederatedMetricsManagerTest, AnswerCardSeenThenListResultLaunched) {
  SetChromeMetricsEnabled(true);
  InitFederatedMetricsManager();

  // Tests that a Launch event takes precedence over an AnswerCardSeen event,
  // within the same search session.
  metrics_manager_->OnSearchSessionStarted();
  std::vector<Result> shown_results;
  std::u16string query = u"fake_query";

  metrics_manager_->OnSeen(Location::kAnswerCard, shown_results, query);

  Result launched_result = CreateFakeResult(Type::EXTENSION_APP, "fake_id");
  metrics_manager_->OnLaunch(Location::kList, launched_result, shown_results,
                             query);

  metrics_manager_->OnSearchSessionEnded(query);
  base::RunLoop().RunUntilIdle();

  const bool launcher_fa_enabled = GetParam();
  if (launcher_fa_enabled) {
    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramInitStatus,
        app_list::federated::FederatedMetricsManager::InitStatus::kOk, 1);

    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramSearchSessionConclusion,
        ash::SearchSessionConclusion::kLaunch, 1);

    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramReportStatus,
        app_list::federated::FederatedMetricsManager::ReportStatus::kOk, 1);
    // TODO(b/262611120): Check contents of logged example, once this
    // functionality is available.
  } else {
    ExpectNoFederatedLogs();
  }
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

TEST_P(FederatedMetricsManagerTest, ZeroState) {
  SetChromeMetricsEnabled(true);
  InitFederatedMetricsManager();

  // Note: metrics_manager_->OnSearchSession{Started,Ended}() are not expected
  // to be called during zero state search.

  // Simulate a series of user actions in zero state search. An empty query
  // indicates zero state search.
  std::vector<Result> shown_results;
  std::u16string empty_query = u"";

  metrics_manager_->OnSeen(Location::kContinue, shown_results, empty_query);
  metrics_manager_->OnSeen(Location::kRecentApps, shown_results, empty_query);

  Result launched_result = CreateFakeResult(Type::EXTENSION_APP, "fake_id");
  metrics_manager_->OnLaunch(Location::kRecentApps, launched_result,
                             shown_results, empty_query);
  base::RunLoop().RunUntilIdle();

  const bool launcher_fa_enabled = GetParam();
  if (launcher_fa_enabled) {
    histogram_tester()->ExpectUniqueSample(
        app_list::federated::kHistogramInitStatus,
        app_list::federated::FederatedMetricsManager::InitStatus::kOk, 1);

    // Zero state search should not trigger any logging on user action.
    histogram_tester()->ExpectTotalCount(
        app_list::federated::kHistogramSearchSessionConclusion, 0);
    histogram_tester()->ExpectTotalCount(
        app_list::federated::kHistogramReportStatus, 0);

    // Do not expect that any examples were logged to the federated service.
    // TODO(b/262611120): Check contents of federated service storage, once this
    // functionality is available.
  } else {
    ExpectNoFederatedLogs();
  }
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

}  // namespace
}  // namespace app_list::test
