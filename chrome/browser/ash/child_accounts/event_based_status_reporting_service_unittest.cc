// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/event_based_status_reporting_service.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/experiences/arc/mojom/app.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@test",
                                            GaiaId::Literal("123456789"));

class TestingConsumerStatusReportingService
    : public ChildStatusReportingService {
 public:
  explicit TestingConsumerStatusReportingService(
      content::BrowserContext* context)
      : ChildStatusReportingService(context) {}

  TestingConsumerStatusReportingService(
      const TestingConsumerStatusReportingService&) = delete;
  TestingConsumerStatusReportingService& operator=(
      const TestingConsumerStatusReportingService&) = delete;

  ~TestingConsumerStatusReportingService() override = default;

  bool RequestImmediateStatusReport() override {
    performed_status_reports_++;
    return true;
  }

  int performed_status_reports() const { return performed_status_reports_; }

 private:
  int performed_status_reports_ = 0;
};

class TestingScreenTimeController : public ScreenTimeController {
 public:
  explicit TestingScreenTimeController(content::BrowserContext* context)
      : ScreenTimeController(context) {}

  TestingScreenTimeController(const TestingScreenTimeController&) = delete;
  TestingScreenTimeController& operator=(const TestingScreenTimeController&) =
      delete;

  ~TestingScreenTimeController() override = default;

  // Override this method so that it doesn't call the StatusUploader instance in
  // ConsumerStatusReportingService, which doesn't exist in these tests.
  base::TimeDelta GetScreenTimeDuration() override { return base::TimeDelta(); }
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
 public:
  EventBasedStatusReportingServiceTest(
      const EventBasedStatusReportingServiceTest&) = delete;
  EventBasedStatusReportingServiceTest& operator=(
      const EventBasedStatusReportingServiceTest&) = delete;

 protected:
  EventBasedStatusReportingServiceTest() = default;
  ~EventBasedStatusReportingServiceTest() override = default;

  void SetUp() override {
    session_manager_ = std::make_unique<session_manager::SessionManager>(
        std::make_unique<session_manager::FakeSessionManagerDelegate>());
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState(),
        /*cros_settings=*/nullptr));
    session_manager_->OnUserManagerCreated(user_manager::UserManager::Get());

    ASSERT_TRUE(user_manager::TestHelper(user_manager::UserManager::Get())
                    .AddRegularUser(kAccountId));

    chromeos::PowerManagerClient::InitializeFake();
    SystemClockClient::InitializeFake();

    arc_app_test_.PreProfileSetUp();

    session_manager_->SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);

    // Simulate log-in.
    session_manager_->CreateSession(
        kAccountId, user_manager::TestHelper::GetFakeUsernameHash(kAccountId),
        /*new_user=*/false,
        /*has_active_session=*/false);

    profile_ = std::make_unique<TestingProfile>();
    ash::AnnotatedAccountId::Set(profile_.get(), kAccountId);
    profile_->SetIsSupervisedProfile();
    // TODO(hidehiko): we should set up kChild account from the beginning,
    // but ArcAppTest does not support such a case. Fix the test helper.
    arc_app_test_.PostProfileSetUp(profile());

    ChildStatusReportingServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&CreateTestingConsumerStatusReportingService));
    ChildStatusReportingService* consumer_status_reporting_service =
        ChildStatusReportingServiceFactory::GetForBrowserContext(profile());
    test_consumer_status_reporting_service_ =
        static_cast<TestingConsumerStatusReportingService*>(
            consumer_status_reporting_service);

    // `ScreenTimeController` depends on `ParentAccessService`.
    parent_access_service_ =
        std::make_unique<parent_access::ParentAccessService>(
            TestingBrowserProcess::GetGlobal()->local_state());

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
    arc_app_test_.PreProfileTearDown();
    profile_.reset();
    arc_app_test_.PostProfileTearDown();
    SystemClockClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();

    // This is following the production teardown order.
    parent_access_service_.reset();
    session_manager_.reset();
    user_manager_.Reset();
  }

  void SetConnectionType(network::mojom::ConnectionType type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        type);
    task_environment_.RunUntilIdle();
  }

  arc::mojom::AppHost* app_host() { return arc_app_test_.arc_app_list_prefs(); }
  Profile* profile() { return profile_.get(); }
  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

  TestingConsumerStatusReportingService*
  test_consumer_status_reporting_service() {
    return test_consumer_status_reporting_service_;
  }

  TestingScreenTimeController* test_screen_time_controller() {
    return test_screen_time_controller_;
  }

  session_manager::SessionManager& session_manager() {
    return CHECK_DEREF(session_manager_.get());
  }

  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager user_manager_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<parent_access::ParentAccessService> parent_access_service_;
  ArcAppTest arc_app_test_{ArcAppTest::UserManagerMode::kDoNothing};
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<TestingConsumerStatusReportingService, DanglingUntriaged>
      test_consumer_status_reporting_service_;
  raw_ptr<TestingScreenTimeController, DanglingUntriaged>
      test_screen_time_controller_;
  std::unique_ptr<EventBasedStatusReportingService> service_;
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
  session_manager().SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());

  histogram_tester_.ExpectTotalCount(
      EventBasedStatusReportingService::kUMAStatusReportEvent, 0);
}

TEST_F(EventBasedStatusReportingServiceTest, ReportWhenSessionIsLocked) {
  ASSERT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager().SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager().SetSessionState(session_manager::SessionState::LOCKED);
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
  session_manager().SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager().SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager().SetSessionState(session_manager::SessionState::ACTIVE);
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
  session_manager().SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_EQ(
      0, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager().SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_EQ(
      1, test_consumer_status_reporting_service()->performed_status_reports());
  session_manager().SetSessionState(session_manager::SessionState::ACTIVE);
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

}  // namespace ash
