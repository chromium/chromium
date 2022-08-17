// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_portal_detector_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/net/network_portal_detector_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "components/account_id/account_id.h"
#include "components/captive_portal/core/captive_portal_testing_utils.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "dbus/object_path.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

class NetworkPortalWebDialog;

namespace {

const char* const kNotificationId =
    NetworkPortalNotificationController::kNotificationId;

constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";
constexpr char kWifiServicePath[] = "/service/wifi";
constexpr char kWifiGuid[] = "wifi";

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(FATAL) << "Shill Error: " << error_name << " : " << error_message;
}

void SetConnected(const std::string& service_path) {
  ShillServiceClient::Get()->Connect(dbus::ObjectPath(service_path),
                                     base::DoNothing(),
                                     base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
}

void SetPortal(const std::string& service_path) {
  ShillServiceClient::Get()->SetProperty(
      dbus::ObjectPath(kWifiServicePath), shill::kStateProperty,
      base::Value(shill::kStateRedirectFound), base::DoNothing(),
      base::BindOnce(&ErrorCallbackFunction));
  base::RunLoop().RunUntilIdle();
}

}  // namespace

class NetworkPortalDetectorImplBrowserTest
    : public LoginManagerTest,
      public captive_portal::CaptivePortalDetectorTestBase {
 public:
  NetworkPortalDetectorImplBrowserTest()
      : LoginManagerTest(),
        test_account_id_(
            AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId)),
        network_portal_detector_(nullptr) {}

  NetworkPortalDetectorImplBrowserTest(
      const NetworkPortalDetectorImplBrowserTest&) = delete;
  NetworkPortalDetectorImplBrowserTest& operator=(
      const NetworkPortalDetectorImplBrowserTest&) = delete;

  ~NetworkPortalDetectorImplBrowserTest() override {}

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService(kWifiServicePath, kWifiGuid, "wifi",
                             shill::kTypeWifi, shill::kStateIdle,
                             true /* add_to_visible */);

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);

    network_portal_detector_ =
        new NetworkPortalDetectorImpl(test_loader_factory());
    // Takes ownership of |network_portal_detector_|:
    network_portal_detector::InitializeForTesting(network_portal_detector_);
    network_portal_detector_->enabled_ = true;
    set_detector(network_portal_detector_->captive_portal_detector_.get());
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
};

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTest,
                       PRE_InSessionDetection) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(NetworkPortalDetectorImplBrowserTest,
                       InSessionDetection) {
  LoginUser(test_account_id_);
  content::RunAllPendingInMessageLoop();

  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));

  // Set connected should not trigger portal detection.
  SetConnected(kWifiServicePath);

  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  const chromeos::NetworkState* default_network =
      network_state_handler->DefaultNetwork();
  ASSERT_TRUE(default_network);
  EXPECT_EQ(default_network->GetPortalState(),
            chromeos::NetworkState::PortalState::kOnline);
  EXPECT_FALSE(display_service_->GetNotification(kNotificationId));
  EXPECT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE,
            network_portal_detector::GetInstance()->GetCaptivePortalStatus());

  // Setting a portal state should set portal detection and display a
  // notification
  SetPortal(kWifiServicePath);

  default_network = network_state_handler->DefaultNetwork();
  ASSERT_TRUE(default_network);
  EXPECT_EQ(default_network->GetPortalState(),
            chromeos::NetworkState::PortalState::kPortal);
  EXPECT_TRUE(display_service_->GetNotification(kNotificationId));
  EXPECT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
            network_portal_detector::GetInstance()->GetCaptivePortalStatus());

  // Explicitly close the notification.
  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       kNotificationId, true);
}

class NetworkPortalDetectorImplBrowserTestIgnoreProxy
    : public NetworkPortalDetectorImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  NetworkPortalDetectorImplBrowserTestIgnoreProxy()
      : NetworkPortalDetectorImplBrowserTest() {}

  NetworkPortalDetectorImplBrowserTestIgnoreProxy(
      const NetworkPortalDetectorImplBrowserTestIgnoreProxy&) = delete;
  NetworkPortalDetectorImplBrowserTestIgnoreProxy& operator=(
      const NetworkPortalDetectorImplBrowserTestIgnoreProxy&) = delete;

 protected:
  void TestImpl(const bool preference_value);
};

void NetworkPortalDetectorImplBrowserTestIgnoreProxy::TestImpl(
    const bool preference_value) {
  LoginUser(test_account_id_);
  content::RunAllPendingInMessageLoop();

  SetIgnoreNoNetworkForTesting();

  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kCaptivePortalAuthenticationIgnoresProxy, preference_value);

  // User connects to portalled wifi.
  SetConnected(kWifiServicePath);
  SetPortal(kWifiServicePath);

  // Check that the network is behind a portal and a notification is displayed.
  EXPECT_TRUE(display_service_->GetNotification(kNotificationId));
  EXPECT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL,
            network_portal_detector::GetInstance()->GetCaptivePortalStatus());

  display_service_->GetNotification(kNotificationId)
      ->delegate()
      ->Click(absl::nullopt, absl::nullopt);

  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(preference_value, static_cast<bool>(GetDialog()));
}

IN_PROC_BROWSER_TEST_P(NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                       PRE_TestWithPreference) {
  RegisterUser(test_account_id_);
  StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_P(NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                       TestWithPreference) {
  TestImpl(GetParam());
}

INSTANTIATE_TEST_SUITE_P(CaptivePortalAuthenticationIgnoresProxy,
                         NetworkPortalDetectorImplBrowserTestIgnoreProxy,
                         testing::Bool());

}  // namespace ash
