// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_encrypted_event_reporter.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/policy/reporting/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_manager.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::_;

namespace policy {

class AppInstallEventEncryptedReporterTest : public testing::Test {
 protected:
  AppInstallEventEncryptedReporterTest() = default;

  AppInstallEventEncryptedReporterTest(
      const AppInstallEventEncryptedReporterTest&) = delete;
  AppInstallEventEncryptedReporterTest& operator=(
      const AppInstallEventEncryptedReporterTest&) = delete;

  void SetUp() override {
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);
    chromeos::PowerManagerClient::InitializeFake();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    chromeos::PowerManagerClient::Shutdown();
  }

  em::AppInstallReportLogEvent CreateEventWithType(
      em::AppInstallReportLogEvent::EventType event_type) {
    auto event = em::AppInstallReportLogEvent();
    event.set_event_type(event_type);
    return event;
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  TestingProfile profile_;
  ash::system::FakeStatisticsProvider statistics_provider_;
};

// Verify that the following event types are reported:
// AppInstallReportLogEvent::SUCCESS
// AppInstallReportLogEvent::INSTALLATION_STARTED
// AppInstallReportLogEvent::INSTALLATION_FAILED
TEST_F(AppInstallEventEncryptedReporterTest, Default) {
  const std::set<std::string> packages = {"test_package"};

  const auto event_success =
      CreateEventWithType(em::AppInstallReportLogEvent::SUCCESS);
  const auto event_started =
      CreateEventWithType(em::AppInstallReportLogEvent::INSTALLATION_STARTED);
  const auto event_failed =
      CreateEventWithType(em::AppInstallReportLogEvent::INSTALLATION_FAILED);

  auto report_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new ::reporting::MockReportQueue(),
          base::OnTaskRunnerDeleter(
              base::ThreadPool::CreateSequencedTaskRunner({})));

  EXPECT_CALL(*report_queue.get(), AddRecord).Times(3);

  auto reporter =
      ArcAppInstallEncryptedEventReporter(std::move(report_queue), &profile_);

  reporter.Add(packages, std::move(event_success));
  reporter.Add(packages, std::move(event_started));
  reporter.Add(packages, std::move(event_failed));
}

}  // namespace policy
