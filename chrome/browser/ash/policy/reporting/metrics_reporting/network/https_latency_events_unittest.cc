// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/fake_network_diagnostics_util.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_event_detector.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using HttpsLatencyProblemMojom =
    ::chromeos::network_diagnostics::mojom::HttpsLatencyProblem;
using RoutineVerdictMojom =
    ::chromeos::network_diagnostics::mojom::RoutineVerdict;

namespace reporting {
namespace {

class HttpsLatencyTestReportQueue : public test::FakeMetricReportQueue {
 public:
  HttpsLatencyTestReportQueue() = default;
  HttpsLatencyTestReportQueue(const HttpsLatencyTestReportQueue&) = delete;
  HttpsLatencyTestReportQueue& operator=(const HttpsLatencyTestReportQueue&) =
      delete;

  ~HttpsLatencyTestReportQueue() override = default;

  // Override to block out unwanted metrics being reported.
  void Enqueue(MetricData metric_data,
               ReportQueue::EnqueueCallback callback) override {
    if (!metric_data.telemetry_data()
             .networks_telemetry()
             .has_https_latency_data()) {
      return;
    }
    test::FakeMetricReportQueue::Enqueue(std::move(metric_data),
                                         std::move(callback));
  }
};

class FakeMetricReportingManagerDelegate
    : public MetricReportingManager::Delegate {
 public:
  FakeMetricReportingManagerDelegate(
      FakeNetworkDiagnostics* fake_diagnostics,
      std::unique_ptr<MetricReportQueue> metric_report_queue)
      : fake_diagnostics_(fake_diagnostics) {
    metric_report_queue_ = std::move(metric_report_queue);
  }

  FakeMetricReportingManagerDelegate(
      const FakeMetricReportingManagerDelegate& other) = delete;
  FakeMetricReportingManagerDelegate& operator=(
      const FakeMetricReportingManagerDelegate& other) = delete;
  ~FakeMetricReportingManagerDelegate() override = default;

  std::unique_ptr<Sampler> GetHttpsLatencySampler() const override {
    return std::make_unique<HttpsLatencySampler>(
        std::make_unique<FakeHttpsLatencyDelegate>(fake_diagnostics_));
  }

  bool IsDeprovisioned() const override { return false; }

  bool IsAppServiceAvailableForProfile(Profile* profile) const override {
    return false;
  }

  std::unique_ptr<MetricReportQueue> CreateMetricReportQueue(
      EventType event_type,
      Destination destination,
      Priority priority,
      std::unique_ptr<RateLimiterInterface> rate_limiter,
      std::optional<SourceInfo> source_info) override {
    if (event_type != EventType::kDevice ||
        destination != Destination::EVENT_METRIC ||
        priority != Priority::SLOW_BATCH) {
      // Return a fake metric report queue so we do not block initialization of
      // other downstream metric reporting components.
      return std::make_unique<test::FakeMetricReportQueue>();
    }

    return std::move(metric_report_queue_);
  }

 private:
  const raw_ptr<FakeNetworkDiagnostics> fake_diagnostics_;

  std::unique_ptr<MetricReportQueue> metric_report_queue_;
};

class HttpsLatencyEventsTest : public ::testing::Test {
 public:
  HttpsLatencyEventsTest() = default;
  HttpsLatencyEventsTest(const HttpsLatencyEventsTest&) = delete;
  HttpsLatencyEventsTest& operator=(const HttpsLatencyEventsTest&) = delete;

  ~HttpsLatencyEventsTest() override = default;

  void SetUp() override {
    // Reporting test environment needs to be created before other
    // initializations.
    reporting_test_enviroment_ =
        reporting::ReportingClient::TestEnvironment::CreateWithStorageModule();

    ::ash::LoginState::Initialize();
    ::ash::DebugDaemonClient::InitializeFake();
    ::ash::cros_healthd::FakeCrosHealthd::Initialize();
  }

  void InitProfile(bool affiliated) {
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager_ = user_manager.get();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    auto account_id = AccountId::FromUserEmail("ini_fan@gmail.com");
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(account_id.GetUserEmail());
    profile_ = profile_builder.Build();
    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, affiliated, user_manager::UserType::kRegular,
        profile_.get());
    user_manager_->LoginUser(account_id, /*set_profile_created_flag=*/false);
  }

  void ProcessProblem(FakeNetworkDiagnostics* diagnostics,
                      HttpsLatencyProblemMojom problem,
                      base::TimeDelta delay) {
    diagnostics->SetResultProblem(problem);
    task_environment_.FastForwardBy(delay);
    diagnostics->ExecuteCallback();
    task_environment_.RunUntilIdle();
  }

  void ProcessNoProblem(FakeNetworkDiagnostics* diagnostics,
                        int latency,
                        base::TimeDelta delay) {
    diagnostics->SetResultNoProblem(latency);
    task_environment_.FastForwardBy(delay);
    diagnostics->ExecuteCallback();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    ::ash::cros_healthd::FakeCrosHealthd::Shutdown();
    ::ash::DebugDaemonClient::Shutdown();
    ::ash::LoginState::Shutdown();

    reporting_test_enviroment_.reset();
  }

  void EnableDeviceNetworkStatusPolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceNetworkStatus, true);
  }

  void DisableDeviceNetworkStatusPolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceNetworkStatus, false);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<reporting::ReportingClient::TestEnvironment>
      reporting_test_enviroment_;

  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;

  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;

  std::unique_ptr<MetricReportQueue> metric_report_queue_;
  raw_ptr<HttpsLatencyTestReportQueue> report_queue_;
};

TEST_F(HttpsLatencyEventsTest, RoutineVerdictProblem) {
  InitProfile(/*affiliated=*/true);
  EnableDeviceNetworkStatusPolicy();

  FakeNetworkDiagnostics diagnostics;
  int latency_ms = 50;
  diagnostics.SetResultNoProblem(latency_ms);

  HttpsLatencyTestReportQueue* fake_event_queue =
      new HttpsLatencyTestReportQueue();
  std::unique_ptr<MetricReportQueue> metric_report_queue_(fake_event_queue);
  auto delegate = std::make_unique<FakeMetricReportingManagerDelegate>(
      &diagnostics, std::move(metric_report_queue_));
  auto init_delay = delegate->GetInitDelay();
  std::unique_ptr<MetricReportingManager> metric_reporting_manager =
      MetricReportingManager::CreateForTesting(std::move(delegate), nullptr);

  metric_reporting_manager->OnLogin(profile_.get());
  ProcessNoProblem(&diagnostics, latency_ms, init_delay);
  EXPECT_TRUE(fake_event_queue->IsEmpty());

  ProcessNoProblem(&diagnostics, latency_ms,
                   metrics::kDefaultNetworkTelemetryEventCheckingRate);
  EXPECT_TRUE(fake_event_queue->IsEmpty());

  // Send a problem after login.
  ProcessProblem(&diagnostics, HttpsLatencyProblemMojom::kFailedHttpsRequests,
                 metrics::kDefaultNetworkTelemetryCollectionRate);
  MetricData metric_result = fake_event_queue->GetMetricDataReported();

  EXPECT_EQ(metric_result.event_data().type(),
            MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE);
  ASSERT_TRUE(metric_result.telemetry_data()
                  .networks_telemetry()
                  .has_https_latency_data());
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .verdict(),
            RoutineVerdict::PROBLEM);
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .latency_ms(),
            latency_ms);
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .problem(),
            HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);

  // Make sure nothing is reported for a continuing problem.
  ProcessProblem(&diagnostics, HttpsLatencyProblemMojom::kFailedHttpsRequests,
                 metrics::kDefaultNetworkTelemetryCollectionRate);
  ASSERT_TRUE(fake_event_queue->IsEmpty());

  // Report when a problem is resolved.
  ProcessNoProblem(&diagnostics, latency_ms,
                   metrics::kDefaultNetworkTelemetryCollectionRate);
  metric_result = fake_event_queue->GetMetricDataReported();
  EXPECT_EQ(metric_result.event_data().type(),
            MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE);
  ASSERT_TRUE(metric_result.telemetry_data()
                  .networks_telemetry()
                  .has_https_latency_data());
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .verdict(),
            RoutineVerdict::NO_PROBLEM);
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .latency_ms(),
            latency_ms);
  EXPECT_FALSE(metric_result.telemetry_data()
                   .networks_telemetry()
                   .https_latency_data()
                   .has_problem());

  // Ensure going from no problem reported to a problem is reported.
  ProcessProblem(&diagnostics, HttpsLatencyProblemMojom::kFailedHttpsRequests,
                 metrics::kDefaultNetworkTelemetryCollectionRate);

  metric_result = fake_event_queue->GetMetricDataReported();
  EXPECT_EQ(metric_result.event_data().type(),
            MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE);
  ASSERT_TRUE(metric_result.telemetry_data()
                  .networks_telemetry()
                  .has_https_latency_data());
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .verdict(),
            RoutineVerdict::PROBLEM);
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .latency_ms(),
            50);
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .problem(),
            HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);
}

TEST_F(HttpsLatencyEventsTest, ReportDeviceNetworkStatusDisabled) {
  InitProfile(/*affiliated=*/true);
  DisableDeviceNetworkStatusPolicy();

  FakeNetworkDiagnostics diagnostics;
  HttpsLatencyTestReportQueue* fake_event_queue =
      new HttpsLatencyTestReportQueue();
  std::unique_ptr<MetricReportQueue> metric_report_queue_(fake_event_queue);
  auto delegate = std::make_unique<FakeMetricReportingManagerDelegate>(
      &diagnostics, std::move(metric_report_queue_));
  std::unique_ptr<MetricReportingManager> metric_reporting_manager =
      MetricReportingManager::CreateForTesting(std::move(delegate), nullptr);
  metric_reporting_manager->OnLogin(profile_.get());

  ProcessProblem(&diagnostics, HttpsLatencyProblemMojom::kFailedHttpsRequests,
                 metrics::kDefaultNetworkTelemetryEventCheckingRate);
  ASSERT_TRUE(fake_event_queue->IsEmpty());
}

TEST_F(HttpsLatencyEventsTest, ReportDeviceNetworkStatusUnaffiliatedUser) {
  InitProfile(/*affiliated=*/false);
  EnableDeviceNetworkStatusPolicy();

  FakeNetworkDiagnostics diagnostics;
  HttpsLatencyTestReportQueue* fake_event_queue =
      new HttpsLatencyTestReportQueue();
  std::unique_ptr<MetricReportQueue> metric_report_queue_(fake_event_queue);
  auto delegate = std::make_unique<FakeMetricReportingManagerDelegate>(
      &diagnostics, std::move(metric_report_queue_));
  std::unique_ptr<MetricReportingManager> metric_reporting_manager =
      MetricReportingManager::CreateForTesting(std::move(delegate), nullptr);
  metric_reporting_manager->OnLogin(profile_.get());

  ProcessProblem(&diagnostics, HttpsLatencyProblemMojom::kFailedHttpsRequests,
                 metrics::kDefaultNetworkTelemetryEventCheckingRate);
  ASSERT_TRUE(fake_event_queue->IsEmpty());
}

TEST_F(HttpsLatencyEventsTest, EventCheckingRateSet) {
  InitProfile(/*affiliated=*/true);
  EnableDeviceNetworkStatusPolicy();
  auto default_rate = metrics::kDefaultNetworkTelemetryEventCheckingRate;
  // Double the default checking rate
  scoped_testing_cros_settings_.device_settings()->SetInteger(
      ::ash::kReportDeviceNetworkTelemetryEventCheckingRateMs,
      default_rate.InMilliseconds() * 2);
  scoped_testing_cros_settings_.device_settings()->SetInteger(
      ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
      default_rate.InMilliseconds());
  int latency_ms = 50;

  FakeNetworkDiagnostics diagnostics;
  HttpsLatencyTestReportQueue* fake_event_queue =
      new HttpsLatencyTestReportQueue();
  std::unique_ptr<MetricReportQueue> metric_report_queue_(fake_event_queue);
  auto delegate = std::make_unique<FakeMetricReportingManagerDelegate>(
      &diagnostics, std::move(metric_report_queue_));
  auto init_delay = delegate->GetInitDelay();
  std::unique_ptr<MetricReportingManager> metric_reporting_manager =
      MetricReportingManager::CreateForTesting(std::move(delegate), nullptr);
  metric_reporting_manager->OnLogin(profile_.get());

  ProcessNoProblem(&diagnostics, latency_ms, init_delay);
  EXPECT_TRUE(fake_event_queue->IsEmpty());

  // The set checking rate is double the default, expect nothing.
  ProcessProblem(&diagnostics, HttpsLatencyProblemMojom::kFailedHttpsRequests,
                 metrics::kDefaultNetworkTelemetryEventCheckingRate);
  EXPECT_TRUE(fake_event_queue->IsEmpty());

  // Now that double the default rate has passed, expect something reported.
  task_environment_.FastForwardBy(
      metrics::kDefaultNetworkTelemetryEventCheckingRate);
  diagnostics.ExecuteCallback();

  MetricData metric_result = fake_event_queue->GetMetricDataReported();
  EXPECT_EQ(metric_result.event_data().type(),
            MetricEventType::NETWORK_HTTPS_LATENCY_CHANGE);
  ASSERT_TRUE(metric_result.telemetry_data()
                  .networks_telemetry()
                  .has_https_latency_data());
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .verdict(),
            RoutineVerdict::PROBLEM);
  EXPECT_EQ(metric_result.telemetry_data()
                .networks_telemetry()
                .https_latency_data()
                .problem(),
            HttpsLatencyProblem::FAILED_HTTPS_REQUESTS);
}

}  // namespace
}  // namespace reporting
