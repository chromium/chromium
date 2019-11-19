// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_utils.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "components/account_id/account_id.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_utils.h"
#include "dbus/object_path.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

class NetworkPortalWebDialog;

namespace {

const char* const kNotificationId =
    NetworkPortalNotificationController::kNotificationId;
const char* const kNotificationMetric =
    NetworkPortalNotificationController::kNotificationMetric;
const char* const kUserActionMetric =
    NetworkPortalNotificationController::kUserActionMetric;

constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";
constexpr char kWifiServicePath[] = "/service/wifi";
constexpr char kWifiGuid[] = "wifi";
constexpr char kProbeUrl[] = "http://play.googleapis.com/generate_204";

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(FATAL) << "Shill Error: " << error_name << " : " << error_message;
}

void SetConnected(const std::string& service_path) {
  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath(service_path), base::DoNothing(),
      base::Bind(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class NetworkPortalDetectorImplBrowserTest
    : public LoginManagerTest,
      public captive_portal::CaptivePortalDetectorTestBase {
 public:
  NetworkPortalDetectorImplBrowserTest()
      : LoginManagerTest(false, true),
        test_account_id_(
            AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId)),
        network_portal_detector_(nullptr) {}

  ~NetworkPortalDetectorImplBrowserTest() override {}

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService(kWifiServicePath, kWifiGuid, "wifi",
                             shill::kTypeEthernet, shill::kStateIdle,
                             true /* add_to_visible */);
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(kWifiServicePath), shill::kStateProperty,
        base::Value(shill::kStateRedirectFound), base::DoNothing(),
        base::Bind(&ErrorCallbackFunction));
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(kWifiServicePath), shill::kProbeUrlProperty,
        base::Value(kProbeUrl), base::DoNothing(),
        base::Bind(&ErrorCallbackFunction));

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);

    network_portal_detector_ =
        new NetworkPortalDetectorImpl(test_loader_factory());
    // Takes ownership of |network_portal_detector_|:
    network_portal_detector::InitializeForTesting(network_portal_detector_);
    network_portal_detector_->Enable(false /* start_detection */);
    set_detector(network_portal_detector_->captive_portal_detector_.get());
    PortalDetectorStrategy::set_delay_till_next_attempt_for_testing(
        base::TimeDelta());
    network_portal_notification_controller_ =
        std::make_unique<NetworkPortalNotificationController>(
            network_portal_detector_);
    base::RunLoop().RunUntilIdle();
  }

  void TearDownOnMainThread() override {
    network_portal_notification_controller_.reset();
  }

  void RestartDetection() {
    network_portal_detector_->StopDetection();
    network_portal_detector_->StartDetection();
    base::RunLoop().RunUntilIdle();
  }

  PortalDetectorStrategy* strategy() {
    return network_portal_detector_->strategy_.get();
  }

  void SetIgnoreNoNetworkForTesting() {
    network_portal_notification_controller_->SetIgnoreNoNetworkForTesting();
  }

  const NetworkPortalWebDialog* GetDialog() const {
    return network_portal_notification_controller_->GetDialogForTesting();
  }

 protected:
  AccountId test_account_id_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_;

 private:
  NetworkPortalDetectorImpl* network_portal_detector_;
  std::unique_ptr<NetworkPortalNotificationController>
      network_portal_notification_controller_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorImplBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTest,
                       PRE_InSessionDetection) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN, strategy()->Id());
}

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTest,
                       InSessionDetection) {
  typedef NetworkPortalNotificationController Controller;

  EnumHistogramChecker ui_checker(
      kNotificationMetric, Controller::NOTIFICATION_METRIC_COUNT, nullptr);
  EnumHistogramChecker action_checker(
      kUserActionMetric, Controller::USER_ACTION_METRIC_COUNT, nullptr);

  LoginUser(test_account_id_);
  content::RunAllPendingInMessageLoop();

  // User connects to wifi.
  SetConnected(kWifiServicePath);

  ASSERT_EQ(PortalDetectorStrategy::STRATEGY_ID_SESSION, strategy()->Id());

  // No notification until portal detection is completed.
  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
  RestartDetection();
  EXPECT_EQ(kProbeUrl, get_probe_url());
  CompleteURLFetch(net::OK, 200, nullptr);

  // Check that wifi is marked as behind the portal and that notification
  // is displayed.
  EXPECT_TRUE(display_service_->GetNotification(kNotificationId));
  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
            network_portal_detector::GetInstance()
                ->GetCaptivePortalState(kWifiGuid)
                .status);

  ASSERT_TRUE(
      ui_checker.Expect(Controller::NOTIFICATION_METRIC_DISPLAYED, 1)->Check());
  ASSERT_TRUE(action_checker.Check());

  // User explicitly closes the notification.
  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       kNotificationId, true);

  ASSERT_TRUE(ui_checker.Check());
  ASSERT_TRUE(
      action_checker.Expect(Controller::USER_ACTION_METRIC_CLOSED, 1)->Check());
}

class NetworkPortalDetectorImplBrowserTestIgnoreProxy
    : public NetworkPortalDetectorImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NetworkPortalDetectorImplBrowserTestIgnoreProxy()
      : NetworkPortalDetectorImplBrowserTest() {}

 protected:
  void TestImpl(const bool preference_value);

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalDetectorImplBrowserTestIgnoreProxy);
};

void NetworkPortalDetectorImplBrowserTestIgnoreProxy::TestImpl(
    const bool preference_value) {
  using Controller = NetworkPortalNotificationController;

  EnumHistogramChecker ui_checker(
      kNotificationMetric, Controller::NOTIFICATION_METRIC_COUNT, nullptr);
  EnumHistogramChecker action_checker(
      kUserActionMetric, Controller::USER_ACTION_METRIC_COUNT, nullptr);

  LoginUser(test_account_id_);
  content::RunAllPendingInMessageLoop();

  SetIgnoreNoNetworkForTesting();

  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kCaptivePortalAuthenticationIgnoresProxy, preference_value);

  // User connects to wifi.
  SetConnected(kWifiServicePath);

  EXPECT_EQ(PortalDetectorStrategy::STRATEGY_ID_SESSION, strategy()->Id());

  // No notification until portal detection is completed.
  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
  RestartDetection();
  CompleteURLFetch(net::OK, 200, nullptr);

  // Check that WiFi is marked as behind a portal and that a notification
  // is displayed.
  ASSERT_TRUE(display_service_->GetNotification(kNotificationId));
  EXPECT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
            network_portal_detector::GetInstance()
                ->GetCaptivePortalState(kWifiGuid)
                .status);

  EXPECT_TRUE(
      ui_checker.Expect(Controller::NOTIFICATION_METRIC_DISPLAYED, 1)->Check());
  EXPECT_TRUE(action_checker.Check());

  display_service_->GetNotification(kNotificationId)
      ->delegate()
      ->Click(base::nullopt, base::nullopt);

  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(preference_value, static_cast<bool>(GetDialog()));
}

IN_PROC_BROWSER_TEST_P(NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                       PRE_TestWithPreference) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
  EXPECT_EQ(PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN, strategy()->Id());
}

IN_PROC_BROWSER_TEST_P(NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                       TestWithPreference) {
  TestImpl(GetParam());
}

INSTANTIATE_TEST_SUITE_P(CaptivePortalAuthenticationIgnoresProxy,
                         NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                         testing::Bool());

}  // namespace chromeos
