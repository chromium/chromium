// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/network/network_portal_notification_controller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/result_catcher.h"
#include "net/base/net_errors.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using chromeos::DBusThreadManager;
using chromeos::NetworkPortalDetector;
using chromeos::NetworkPortalDetectorImpl;
using chromeos::NetworkPortalNotificationController;
using chromeos::ShillDeviceClient;
using chromeos::ShillProfileClient;
using chromeos::ShillServiceClient;

namespace {

const char kWifiDevicePath[] = "/device/stub_wifi_device1";
const char kWifi1ServicePath[] = "stub_wifi1";
const char kWifi1ServiceGUID[] = "wifi1_guid";

}  // namespace

class NetworkingConfigTest
    : public extensions::ExtensionApiTest,
      public captive_portal::CaptivePortalDetectorTestBase {
 public:
  NetworkingConfigTest() : network_portal_detector_(nullptr) {}
  ~NetworkingConfigTest() override = default;

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();

    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        nullptr /* profile */);

    DBusThreadManager* const dbus_manager = DBusThreadManager::Get();
    ShillServiceClient::TestInterface* const service_test =
        dbus_manager->GetShillServiceClient()->GetTestInterface();
    ShillDeviceClient::TestInterface* const device_test =
        dbus_manager->GetShillDeviceClient()->GetTestInterface();
    ShillProfileClient::TestInterface* const profile_test =
        dbus_manager->GetShillProfileClient()->GetTestInterface();

    device_test->ClearDevices();
    service_test->ClearServices();

    device_test->AddDevice(kWifiDevicePath, shill::kTypeWifi,
                           "stub_wifi_device1");

    service_test->AddService(kWifi1ServicePath, kWifi1ServiceGUID, "wifi1",
                             shill::kTypeWifi, shill::kStateOnline,
                             true /* add_to_visible */);
    service_test->SetServiceProperty(kWifi1ServicePath, shill::kWifiBSsid,
                                     base::Value("01:02:ab:7f:90:00"));
    service_test->SetServiceProperty(
        kWifi1ServicePath, shill::kSignalStrengthProperty, base::Value(40));
    profile_test->AddService(ShillProfileClient::GetSharedProfilePath(),
                             kWifi1ServicePath);

    content::RunAllPendingInMessageLoop();

    network_portal_detector_ =
        new NetworkPortalDetectorImpl(test_loader_factory());
    // Takes ownership of |network_portal_detector_|:
    chromeos::network_portal_detector::InitializeForTesting(
        network_portal_detector_);
    network_portal_detector_->Enable(false /* start_detection */);
    set_detector(network_portal_detector_->captive_portal_detector_.get());
    network_portal_notification_controller_ =
        std::make_unique<NetworkPortalNotificationController>(
            network_portal_detector_);
  }

  void TearDownOnMainThread() override {
    network_portal_notification_controller_.reset();
  }

  void LoadTestExtension() {
    extension_ = LoadExtension(test_data_dir_.AppendASCII("networking_config"));
  }

  bool RunExtensionTest(const std::string& path) {
    return RunExtensionSubtest("networking_config",
                               extension_->GetResourceURL(path).spec());
  }

  void SimulateCaptivePortal() {
    network_portal_detector_->StartDetection();
    content::RunAllPendingInMessageLoop();

    // Simulate a captive portal reply.
    CompleteURLFetch(net::OK, 200, nullptr);
  }

  void SimulateSuccessfulCaptivePortalAuth() {
    content::RunAllPendingInMessageLoop();
    CompleteURLFetch(net::OK, 204, nullptr);
  }

  NetworkPortalDetector::CaptivePortalStatus GetCaptivePortalStatus(
      const std::string& guid) {
    return network_portal_detector_->GetCaptivePortalState(kWifi1ServiceGUID)
        .status;
  }

 protected:
  NetworkPortalDetectorImpl* network_portal_detector_;
  std::unique_ptr<NetworkPortalNotificationController>
      network_portal_notification_controller_;
  const extensions::Extension* extension_ = nullptr;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

IN_PROC_BROWSER_TEST_F(NetworkingConfigTest, ApiAvailability) {
  ASSERT_TRUE(RunExtensionSubtest("networking_config", "api_availability.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NetworkingConfigTest, RegisterNetworks) {
  ASSERT_TRUE(
      RunExtensionSubtest("networking_config", "register_networks.html"))
      << message_;
}

// Test the full, positive flow starting with the extension registration and
// ending with the captive portal being authenticated.
IN_PROC_BROWSER_TEST_F(NetworkingConfigTest, FullTest) {
  LoadTestExtension();
  // This will cause the extension to register for wifi1.
  ASSERT_TRUE(RunExtensionTest("full_test.html")) << message_;

  SimulateCaptivePortal();

  // Wait until a captive portal notification is displayed and verify that it is
  // the expected captive portal notification.
  auto notification = display_service_->GetNotification(
      NetworkPortalNotificationController::kNotificationId);
  ASSERT_TRUE(notification);

  // Simulate the user click which leads to the extension being notified.
  notification->delegate()->Click(
      NetworkPortalNotificationController::kUseExtensionButtonIndex,
      base::nullopt);

  extensions::ResultCatcher catcher;
  EXPECT_TRUE(catcher.GetNextResult());

  // Simulate the captive portal vanishing.
  SimulateSuccessfulCaptivePortalAuth();

  ASSERT_EQ(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE,
            GetCaptivePortalStatus(kWifi1ServiceGUID));
}
