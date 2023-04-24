// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_portal_detector_impl.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/net/network_portal_detector_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/captive_portal/core/captive_portal_testing_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/object_path.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ElementsAre;
using ::testing::Mock;

// Service path / guid for stub networks.
const char kStubEthernet[] = "stub_ethernet";
const char kStubWireless1[] = "stub_wifi1";
const char kStubWireless2[] = "stub_wifi2";
const int kStatusCodeUnset = -1;

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

}  // namespace

class NetworkPortalDetectorImplTest
    : public testing::Test,
      public captive_portal::CaptivePortalDetectorTestBase {
 protected:
  using State = NetworkPortalDetectorImpl::State;

  NetworkPortalDetectorImplTest()
      : test_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    FakeChromeUserManager* user_manager = new FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager));

    ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    SetupNetworkHandler();

    ASSERT_TRUE(test_profile_manager_.SetUp());

    // Add a user.
    const AccountId test_account_id(
        AccountId::FromUserEmail("test-user@example.com"));
    user_manager->AddUser(test_account_id);
    user_manager->LoginUser(test_account_id);

    // Create a profile for the user.
    profile_ = test_profile_manager_.CreateTestingProfile(
        test_account_id.GetUserEmail());
    EXPECT_TRUE(user_manager::UserManager::Get()->GetPrimaryUser());

    network_portal_detector_ =
        std::make_unique<NetworkPortalDetectorImpl>(test_loader_factory());
    network_portal_detector_->enabled_ = true;

    set_detector(network_portal_detector_->captive_portal_detector_.get());
  }

  void TearDown() override {
    network_portal_detector_.reset();
    profile_ = nullptr;
    network_handler_test_helper_.reset();
    ConciergeClient::Shutdown();
  }

  bool CheckPortalState(NetworkPortalDetector::CaptivePortalStatus status,
                        int response_code,
                        const std::string& guid) {
    NetworkPortalDetector::CaptivePortalStatus detector_status =
        network_portal_detector()->GetCaptivePortalStatus();
    int detector_response_code =
        network_portal_detector()->response_code_for_testing();
    std::string default_network_id =
        network_portal_detector()->default_network_id_for_testing();
    EXPECT_EQ(status, detector_status);
    EXPECT_EQ(response_code, detector_response_code);
    EXPECT_EQ(guid, default_network_id);
    return status == detector_status &&
           response_code == detector_response_code &&
           guid == default_network_id;
  }

  Profile* profile() { return profile_; }

  NetworkPortalDetectorImpl* network_portal_detector() {
    return network_portal_detector_.get();
  }

  NetworkPortalDetectorImpl::State state() {
    return network_portal_detector()->state();
  }

  NetworkPortalDetector::CaptivePortalStatus status() {
    return network_portal_detector()->GetCaptivePortalStatus();
  }

  void StopDetection() { network_portal_detector()->StopDetection(); }

  bool attempt_timeout_is_cancelled() {
    return network_portal_detector()->AttemptTimeoutIsCancelledForTesting();
  }

  void set_attempt_delay(const base::TimeDelta& delay) {
    network_portal_detector()->set_attempt_delay_for_testing(delay);
  }

  void set_attempt_timeout(const base::TimeDelta& timeout) {
    network_portal_detector()->set_attempt_timeout_for_testing(timeout);
  }

  const base::TimeDelta& next_attempt_delay() {
    return network_portal_detector()->next_attempt_delay_for_testing();
  }

  int captive_portal_detector_run_count() {
    return network_portal_detector()
        ->captive_portal_detector_run_count_for_testing();
  }

  void SetNetworkState(const std::string& service_path,
                       const std::string& state) {
    ShillServiceClient::Get()->SetProperty(
        dbus::ObjectPath(service_path), shill::kStateProperty,
        base::Value(state), base::DoNothing(),
        base::BindOnce(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  void SetNetworkDeviceEnabled(const std::string& type, bool enabled) {
    NetworkHandler::Get()
        ->technology_state_controller()
        ->SetTechnologiesEnabled(NetworkTypePattern::Primitive(type), enabled,
                                 network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetConnected(const std::string& service_path) {
    ShillServiceClient::Get()->Connect(dbus::ObjectPath(service_path),
                                       base::DoNothing(),
                                       base::BindOnce(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  // Set a proxy to trigger Chrome portal detection when the connection state
  // is 'online'.
  void SetConnectedWithProxy(const std::string& service_path) {
    SetConnected(service_path);
    std::string proxy_config = "{\"mode\":\"test\"}";
    ShillServiceClient::Get()->SetProperty(
        dbus::ObjectPath(service_path), shill::kProxyConfigProperty,
        base::Value(proxy_config), base::DoNothing(),
        base::BindOnce(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  void SetDisconnected(const std::string& service_path) {
    ShillServiceClient::Get()->Disconnect(
        dbus::ObjectPath(service_path), base::DoNothing(),
        base::BindOnce(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  std::string GetRetryResponse(int retry_delay) {
    return base::StringPrintf("HTTP/1.1 503 OK\nRetry-After: %d\n\n",
                              retry_delay);
  }

  void StartDetection() {
    network_portal_detector_->StartDetectionForTesting();
  }

 private:
  void AddService(const std::string& network_id, const std::string& type) {
    network_handler_test_helper_->service_test()->AddService(
        network_id /* service_path */, network_id /* guid */,
        network_id /* name */, type, shill::kStateIdle,
        true /* add_to_visible */);
  }

  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();
    network_handler_test_helper_->service_test()->ClearServices();
    AddService(kStubEthernet, shill::kTypeEthernet);
    AddService(kStubWireless1, shill::kTypeWifi);
    AddService(kStubWireless2, shill::kTypeWifi);
  }

  void SetupNetworkHandler() {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    SetupDefaultShillState();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
  std::unique_ptr<NetworkPortalDetectorImpl> network_portal_detector_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  TestingProfileManager test_profile_manager_;
};

TEST_F(NetworkPortalDetectorImplTest, NoPortal) {
  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(status(), NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN);

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // Check HTTP 204 response code.
  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                       kStubWireless1));
}

TEST_F(NetworkPortalDetectorImplTest, Portal200) {
  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(status(), NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN);

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // Check HTTP 200 response code.
  CompleteURLFetch(net::OK, 200, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                       kStubWireless1));
}

TEST_F(NetworkPortalDetectorImplTest, Portal302) {
  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(status(), NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN);

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // Check HTTP 302 response code.
  CompleteURLFetch(net::OK, 302, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 302,
                       kStubWireless1));
}

TEST_F(NetworkPortalDetectorImplTest, Online2Offline) {
  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(status(), NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN);

  // WiFi is in online state with a proxy configured to trigger Chrome portal
  // detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());

  EXPECT_EQ(status(), NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);

  // WiFi is turned off.
  SetDisconnected(kStubWireless1);
  EXPECT_EQ(State::STATE_IDLE, state());

  EXPECT_EQ(status(), NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
}

TEST_F(NetworkPortalDetectorImplTest, NetworkChanged) {
  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(status(), NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN);

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);

  // WiFi is in portal state.
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // Active network is changed during portal detection for WiFi.
  SetConnected(kStubEthernet);

  // Portal detection for WiFi is cancelled, portal detection for
  // ethernet is initiated.
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubEthernet));
}

TEST_F(NetworkPortalDetectorImplTest, NetworkStateReconnect) {
  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(status(), NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN);

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 204, nullptr);

  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                       kStubWireless1));

  // Reconnecting to the same network will trigger another portal check with the
  // same results.
  SetDisconnected(kStubWireless1);
  set_attempt_delay(base::TimeDelta());
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 204, nullptr);

  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                       kStubWireless1));
}

TEST_F(NetworkPortalDetectorImplTest, NetworkStateChanged) {
  // Test for Portal -> Online -> Portal network state transitions.
  ASSERT_EQ(State::STATE_IDLE, state());

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 200, nullptr);

  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                       kStubWireless1));

  // Setting the state to portal-suspected should trigger chrome detection.
  set_attempt_delay(base::TimeDelta());
  SetNetworkState(kStubWireless1, shill::kStatePortalSuspected);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // Chreme detects that the network is online.
  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                       kStubWireless1));

  // Setting the state back  to online should trigger chrome detection since a
  // proxy is configured.
  set_attempt_delay(base::TimeDelta());
  SetNetworkState(kStubWireless1, shill::kStateOnline);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // Chreme detects that the network is in a portal state.
  CompleteURLFetch(net::OK, 200, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200,
                       kStubWireless1));
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionTimeout) {
  ASSERT_EQ(State::STATE_IDLE, state());

  // For instantaneous timeout.
  set_attempt_timeout(base::Seconds(0));

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, captive_portal_detector_run_count());

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);

  // First portal detection times out, next portal detection is scheduled.
  EXPECT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  EXPECT_EQ(1, captive_portal_detector_run_count());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionRetryAfter) {
  ASSERT_EQ(State::STATE_IDLE, state());

  const int retry_delay = 101;
  std::string retry_response = GetRetryResponse(retry_delay);

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, captive_portal_detector_run_count());

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
  CompleteURLFetch(net::OK, 503, retry_response.c_str());

  // First portal detection completed, next portal detection is
  // scheduled after |retry_delay| seconds.
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  ASSERT_EQ(1, captive_portal_detector_run_count());
  ASSERT_EQ(base::Seconds(retry_delay), next_attempt_delay());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionRetryAfterIsSmall) {
  ASSERT_EQ(State::STATE_IDLE, state());

  const int retry_delay = 1;
  std::string retry_response = GetRetryResponse(retry_delay);

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, captive_portal_detector_run_count());

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  CompleteURLFetch(net::OK, 503, retry_response.c_str());

  // First portal detection completed, next portal detection is
  // scheduled after 3 seconds (due to minimum time between detection
  // attemps).
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  ASSERT_EQ(1, captive_portal_detector_run_count());
}

TEST_F(NetworkPortalDetectorImplTest, FirstAttemptFailed) {
  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, captive_portal_detector_run_count());
  base::HistogramTester histogram_tester;

  set_attempt_delay(base::TimeDelta());
  const int retry_delay = 0;
  std::string retry_response = GetRetryResponse(retry_delay);

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 503, retry_response.c_str());
  EXPECT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  EXPECT_EQ(1, captive_portal_detector_run_count());
  EXPECT_EQ(base::Seconds(retry_delay), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                       kStubWireless1));

  // Metric records the number of probes.
  histogram_tester.ExpectUniqueSample("Network.NetworkPortalDetectorRunCount",
                                      2, 1);

  // Start a new probe.
  StartDetection();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                       kStubWireless1));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Network.NetworkPortalDetectorRunCount"),
      ElementsAre(base::Bucket(1, 1), base::Bucket(2, 1)));
}

TEST_F(NetworkPortalDetectorImplTest, AllAttemptsFailed) {
  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, captive_portal_detector_run_count());
  base::HistogramTester histogram_tester;

  set_attempt_delay(base::TimeDelta());
  const int retry_delay = 0;
  std::string retry_response = GetRetryResponse(retry_delay);

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 503, retry_response.c_str());
  EXPECT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  EXPECT_EQ(1, captive_portal_detector_run_count());
  EXPECT_EQ(base::Seconds(retry_delay), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_response.c_str());
  EXPECT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  EXPECT_EQ(2, captive_portal_detector_run_count());
  EXPECT_EQ(base::Seconds(retry_delay), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  // Maximum retries will be hit, state should be idle.
  CompleteURLFetch(net::OK, 503, retry_response.c_str());
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE,
                       503, kStubWireless1));

  // Metric records the number of probes.
  histogram_tester.ExpectUniqueSample("Network.NetworkPortalDetectorRunCount",
                                      3, 1);

  // Start a new probe that succeeds.
  StartDetection();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204,
                       kStubWireless1));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Network.NetworkPortalDetectorRunCount"),
      ElementsAre(base::Bucket(1, 1), base::Bucket(3, 1)));
}

TEST_F(NetworkPortalDetectorImplTest, ProxyAuthRequired) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_attempt_delay(base::TimeDelta());

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 407, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED, 407,
      kStubWireless1));
}

TEST_F(NetworkPortalDetectorImplTest, NoResponseButBehindPortal) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_attempt_delay(base::TimeDelta());

  // Set a portal-suspected state to trigger Chrome portal detection.
  SetNetworkState(kStubWireless1, shill::kStatePortalSuspected);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED, 0, nullptr);
  EXPECT_EQ(State::STATE_IDLE, state());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 0, kStubWireless1));
}

TEST_F(NetworkPortalDetectorImplTest, DetectionTimeoutIsCancelled) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_attempt_delay(base::TimeDelta());

  // Connect with a proxy to trigger Chrome portal detection.
  SetConnectedWithProxy(kStubWireless1);
  EXPECT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE,
                       kStatusCodeUnset, kStubWireless1));

  // Stop Chrome portal detection before it completes, the attempt should be
  // cancelled and the result 'unknown'.
  StopDetection();

  EXPECT_EQ(State::STATE_IDLE, state());
  EXPECT_TRUE(attempt_timeout_is_cancelled());
  EXPECT_TRUE(
      CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN,
                       kStatusCodeUnset, kStubWireless1));
}

}  // namespace ash
