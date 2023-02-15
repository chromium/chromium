// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/federated_metrics_manager.h"

#include <memory>

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

class FederatedMetricsManagerTest : public testing::Test {
 public:
  FederatedMetricsManagerTest()
      : scoped_fake_for_test_(&fake_service_connection_),
        app_list_notifier_(&app_list_controller_) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{search_features::
                                  kLauncherQueryFederatedAnalyticsPHH},
        /*disabled_features=*/{});
  }

  FederatedMetricsManagerTest(const FederatedMetricsManagerTest&) = delete;
  FederatedMetricsManagerTest& operator=(const FederatedMetricsManagerTest&) =
      delete;

  // testing::Test:
  void SetUp() override {
    // Start a new IO thread to run IPC tasks.
    io_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    mojo::core::Init();
    ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
        io_thread_.task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

    // Set up federated service connection.
    FederatedClient::InitializeFake();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
    metrics_manager_ = std::make_unique<FederatedMetricsManager>(
        &app_list_notifier_, &federated_service_controller_);
  }

  void TearDown() override {
    FederatedClient::Shutdown();
    ipc_support_.reset();
    io_thread_.Stop();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<FederatedMetricsManager> metrics_manager_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::Thread io_thread_{"IoThread"};
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;

  FakeServiceConnectionImpl fake_service_connection_;
  ScopedFakeServiceConnectionForTest scoped_fake_for_test_;

  ::test::TestAppListController app_list_controller_;
  AppListNotifierImpl app_list_notifier_;
  TestFederatedServiceController federated_service_controller_;
};

// TODO(crbug.com/1416382): Test is flaky on sanitizers.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OnAbandon DISABLED_OnAbandon
#else
#define MAYBE_OnAbandon OnAbandon
#endif
TEST_F(FederatedMetricsManagerTest, MAYBE_OnAbandon) {
  Location location = Location::kList;
  std::vector<Result> shown_results;
  metrics_manager_->OnAbandon(location, shown_results, u"fake_query");
  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectUniqueSample(
      app_list::federated::kHistogramInitStatus,
      app_list::federated::FederatedMetricsManager::InitStatus::kOk, 1);

  histogram_tester()->ExpectUniqueSample(
      app_list::federated::kHistogramAction,
      app_list::federated::FederatedMetricsManager::Action::kAbandon, 1);

  histogram_tester()->ExpectUniqueSample(
      app_list::federated::kHistogramReportStatus,
      app_list::federated::FederatedMetricsManager::ReportStatus::kOk, 1);

  // TODO(b/262611120): Check contents of logged example, once this
  // functionality is available.
}

// TODO(crbug.com/1416382): Test is flaky on sanitizers.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OnLaunch DISABLED_OnLaunch
#else
#define MAYBE_OnLaunch OnLaunch
#endif
TEST_F(FederatedMetricsManagerTest, MAYBE_OnLaunch) {
  Location location = Location::kList;
  std::vector<Result> shown_results;
  Result launched_result = CreateFakeResult(Type::EXTENSION_APP, "fake_id");
  metrics_manager_->OnLaunch(location, launched_result, shown_results,
                             u"fake_query");
  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectUniqueSample(
      app_list::federated::kHistogramInitStatus,
      app_list::federated::FederatedMetricsManager::InitStatus::kOk, 1);

  histogram_tester()->ExpectUniqueSample(
      app_list::federated::kHistogramAction,
      app_list::federated::FederatedMetricsManager::Action::kLaunch, 1);

  histogram_tester()->ExpectUniqueSample(
      app_list::federated::kHistogramReportStatus,
      app_list::federated::FederatedMetricsManager::ReportStatus::kOk, 1);
  // TODO(b/262611120): Check contents of logged example, once this
  // functionality is available.
}

// TODO(crbug.com/1416382): Test is flaky on sanitizers.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ZeroState DISABLED_ZeroState
#else
#define MAYBE_ZeroState ZeroState
#endif
TEST_F(FederatedMetricsManagerTest, MAYBE_ZeroState) {
  Location location = Location::kList;
  std::vector<Result> shown_results;
  Result launched_result = CreateFakeResult(Type::EXTENSION_APP, "fake_id");

  // Simulate a series of user actions in zero state search. An empty query
  // indicates zero state search.
  std::u16string empty_query = u"";
  metrics_manager_->OnAbandon(location, shown_results, empty_query);
  metrics_manager_->OnLaunch(location, launched_result, shown_results,
                             empty_query);
  base::RunLoop().RunUntilIdle();

  histogram_tester()->ExpectUniqueSample(
      app_list::federated::kHistogramInitStatus,
      app_list::federated::FederatedMetricsManager::InitStatus::kOk, 1);

  // Zero state search should not trigger any logging on user action.
  histogram_tester()->ExpectTotalCount(app_list::federated::kHistogramAction,
                                       0);
  histogram_tester()->ExpectTotalCount(
      app_list::federated::kHistogramReportStatus, 0);

  // Do not expect that any examples were logged to the federated service.
  // TODO(b/262611120): Check contents of federated service storage, once this
  // functionality is available.
}

}  // namespace
}  // namespace app_list::test
