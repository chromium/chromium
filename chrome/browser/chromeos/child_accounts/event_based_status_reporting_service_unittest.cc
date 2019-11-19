// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/event_based_status_reporting_service.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/child_status_reporting_service.h"
#include "chrome/browser/chromeos/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "components/account_id/account_id.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class TestingConsumerStatusReportingService
    : public ChildStatusReportingService {
 public:
  explicit TestingConsumerStatusReportingService(
      content::BrowserContext* context)
      : ChildStatusReportingService(context) {}
  ~TestingConsumerStatusReportingService() override = default;

  bool RequestImmediateStatusReport() override {
    performed_status_reports_++;
    return true;
  }

  int performed_status_reports() const { return performed_status_reports_; }

 private:
  int performed_status_reports_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestingConsumerStatusReportingService);
};

class TestingScreenTimeController : public ScreenTimeController {
 public:
  explicit TestingScreenTimeController(content::BrowserContext* context)
      : ScreenTimeController(context) {}
  ~TestingScreenTimeController() override = default;

  // Override this method so that it doesn't call the StatusUploader instance in
  // ConsumerStatusReportingService, which doesn't exist in these tests.
  base::TimeDelta GetScreenTimeDuration() override { return base::TimeDelta(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingScreenTimeController);
};

std::unique_ptr<KeyedService> CreateTestingConsumerStatusReportingService(
    content::BrowserContext* browser_context) {
  return std::unique_ptr<KeyedService>(
      new TestingConsumerStatusReportingService(browser_context));
}

std::unique_ptr<KeyedService> CreateTestingScreenTimeController(
    content::BrowserContext* browser_context) {
  return std::unique_ptr<KeyedService>(
      new TestingScreenTimeController(browser_context));
}

}  // namespace

class EventBasedStatusReportingServiceTest : public testing::Test {
 protected:
  EventBasedStatusReportingServiceTest() = default;
  ~EventBasedStatusReportingServiceTest() override = default;

  void SetUp() override {
    PowerManagerClient::InitializeFake();
    SystemClockClient::InitializeFake();

    // TODO(agawronska): To enable this we need LoginScreenClient, but it causes
    // test crashes on network connection change.
    scoped_feature_list_.InitAndDisableFeature(features::kParentAccessCode);

    profile_ = std::make_unique<TestingProfile>();
    profile_.get()->SetSupervisedUserId(supervised_users::kChildAccountSUID);
    arc_test_.SetUp(profile());

    session_manager_.CreateSession(
        account_id(),
        chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
            account_id().GetUserEmail()),
        true);
    session_manager_.SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);

    ChildStatusReportingServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&CreateTestingConsumerStatusReportingService));
    ChildStatusReportingService* consumer_status_reporting_service =
        ChildStatusReportingServiceFactory::GetForBrowserContext(profile());
    test_consumer_status_reporting_service_ =
        static_cast<TestingConsumerStatusReportingService*>(
            consumer_status_reporting_service);

    ScreenTimeControllerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&CreateTestingScreenTimeController));
    ScreenTimeController* screen_time_controller =
        ScreenTimeControllerFactory::GetForBrowserContext(profile());
    test_screen_time_controller_ =
        static_cast<TestingScreenTimeController*>(screen_time_controller);
    service_ =
        std::make_unique<EventBasedStatusReportingService>(profile_.get());
  }

  void TearDown() override {
    service_->Shutdown();
    arc_test_.TearDown();
    profile_.reset();
    SystemClockClient::Shutdown();
    PowerManagerClient::Shutdown();
  }

  void SetConnectionType(network::mojom::ConnectionType type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        type);
    task_environment_.RunUntilIdle();
  }

  arc::mojom::AppHost* app_host() { return arc_test_.arc_app_list_prefs(); }
  Profile* profile() { return profile_.get(); }
  FakePowerManagerClient* power_manager_client() {
    return FakePowerManagerClient::Get();
  }

  TestingConsumerStatusReportingService*
  test_consumer_status_reporting_service() {
    return test_consumer_status_reporting_service_;
  }

  TestingScreenTimeController* test_screen_time_controller() {
    return test_screen_time_controller_;
  }

  session_manager::SessionManager* session_manager() {
    return &session_manager_;
  }

  AccountId account_id() {
    return chromeos::ProfileHelper::Get()
        ->GetUserByProfile(profile())
        ->GetAccountId();
  }

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcAppTest arc_test_;
  std::unique_ptr<TestingProfile> profile_;
  TestingConsumerStatusReportingService*
      test_consumer_status_reporting_service_;
  TestingScreenTimeController* test_screen_time_controller_;
  session_manager::SessionManager session_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<EventBasedStatusReportingService> service_;

  DISALLOW_COPY_AND_ASSIGN(EventBasedStatusReportingServiceTest);
};

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenAppInstall) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kAppInstalled, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenAppUpdate) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageModified(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kAppUpdated, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, DoNotReportWhenUserJustSignIn) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 0);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSessionIsLocked) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionLocked, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSessionIsActive) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      2, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionActive, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionLocked, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 2);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenDeviceGoesOnline) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kDeviceOnline, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSuspendIsDone) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  power_manager_client()->SendSuspendDone();
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSuspendDone, 1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportOnUsageTimeLimitWarning) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  test_screen_time_controller()->NotifyUsageTimeLimitWarningForTesting();
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::
          kUsageTimeLimitWarning,
      1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 1);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportForMultipleEvents) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager()->SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      2, test_consumer_status_reporting_service()->performed_status_reports());
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(
      3, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      4, test_consumer_status_reporting_service()->performed_status_reports());
  app_host()->OnPackageModified(arc::mojom::ArcPackageInfo::New());
  EXPECT_EQ(
      5, test_consumer_status_reporting_service()->performed_status_reports());
  power_manager_client()->SendSuspendDone();
  EXPECT_EQ(
      6, test_consumer_status_reporting_service()->performed_status_reports());
  test_screen_time_controller()->NotifyUsageTimeLimitWarningForTesting();
  EXPECT_EQ(
      7, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionLocked, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSessionActive, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kDeviceOnline, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kAppInstalled, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kAppUpdated, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::kSuspendDone, 1);
  histogram_tester_.ExpectBucketCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent,
      EventBasedStatusReportingService::StatusReportEvent::
          kUsageTimeLimitWarning,
      1);
  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 7);
}

}  // namespace chromeos
