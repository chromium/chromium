// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector_impl.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/captive_portal/captive_portal_testing_utils.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/object_path.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::AnyNumber;
using testing::Mock;
using testing::_;

namespace chromeos {

namespace {

// Service path / guid for stub networks.
const char kStubEthernet[] = "stub_ethernet";
const char kStubWireless1[] = "stub_wifi1";
const char kStubWireless2[] = "stub_wifi2";
const char kStubCellular[] = "stub_cellular";

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

class MockObserver : public NetworkPortalDetector::Observer {
 public:
  virtual ~MockObserver() {}

  MOCK_METHOD2(OnPortalDetectionCompleted,
               void(const NetworkState* network,
                    const NetworkPortalDetector::CaptivePortalState& state));
};

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

    DBusThreadManager::Initialize();
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

    network_portal_detector_.reset(
        new NetworkPortalDetectorImpl(test_loader_factory()));
    network_portal_detector_->Enable(false);

    set_detector(network_portal_detector_->captive_portal_detector_.get());

    // Prevents flakiness due to message loop delays.
    set_time_ticks(base::TimeTicks::Now());

    if (base::HistogramBase* histogram =
            base::StatisticsRecorder::FindHistogram(
                "CaptivePortal.OOBE.DetectionResult")) {
      original_samples_ = histogram->SnapshotSamples();
    }
  }

  void TearDown() override {
    network_portal_detector_.reset();
    profile_ = nullptr;
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
    PortalDetectorStrategy::reset_fields_for_testing();
  }

  void CheckPortalState(NetworkPortalDetector::CaptivePortalStatus status,
                        int response_code,
                        const std::string& guid) {
    NetworkPortalDetector::CaptivePortalState state =
        network_portal_detector()->GetCaptivePortalState(guid);
    ASSERT_EQ(status, state.status);
    ASSERT_EQ(response_code, state.response_code);
  }

  void CheckRequestTimeoutAndCompleteAttempt(
      int expected_same_detection_result_count,
      int expected_no_response_result_count,
      int expected_request_timeout_sec,
      int net_error,
      int status_code) {
    ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
    ASSERT_EQ(expected_same_detection_result_count,
              same_detection_result_count());
    ASSERT_EQ(expected_no_response_result_count, no_response_result_count());
    ASSERT_EQ(base::TimeDelta::FromSeconds(expected_request_timeout_sec),
              get_next_attempt_timeout());
    CompleteURLFetch(net_error, status_code, nullptr);
  }

  Profile* profile() { return profile_; }

  NetworkPortalDetectorImpl* network_portal_detector() {
    return network_portal_detector_.get();
  }

  void AddObserver(NetworkPortalDetector::Observer* observer) {
    network_portal_detector()->AddObserver(observer);
  }

  void RemoveObserver(NetworkPortalDetector::Observer* observer) {
    network_portal_detector()->RemoveObserver(observer);
  }

  NetworkPortalDetectorImpl::State state() {
    return network_portal_detector()->state();
  }

  bool StartPortalDetection(bool force) {
    return network_portal_detector()->StartPortalDetection(force);
  }

  void enable_error_screen_strategy() {
    network_portal_detector()->SetStrategy(
        PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN);
  }

  void disable_error_screen_strategy() {
    network_portal_detector()->SetStrategy(
        PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN);
  }

  void stop_detection() { network_portal_detector()->StopDetection(); }

  bool attempt_timeout_is_cancelled() {
    return network_portal_detector()->AttemptTimeoutIsCancelledForTesting();
  }

  base::TimeDelta get_next_attempt_timeout() {
    return network_portal_detector()->strategy_->GetNextAttemptTimeout();
  }

  void set_next_attempt_timeout(const base::TimeDelta& timeout) {
    PortalDetectorStrategy::set_next_attempt_timeout_for_testing(timeout);
  }

  const base::TimeDelta& next_attempt_delay() {
    return network_portal_detector()->next_attempt_delay_for_testing();
  }

  int same_detection_result_count() {
    return network_portal_detector()->same_detection_result_count_for_testing();
  }

  int no_response_result_count() {
    return network_portal_detector()->no_response_result_count_for_testing();
  }

  void set_no_response_result_count(int count) {
    network_portal_detector()->set_no_response_result_count_for_testing(count);
  }

  void set_delay_till_next_attempt(const base::TimeDelta& delta) {
    PortalDetectorStrategy::set_delay_till_next_attempt_for_testing(delta);
  }

  void set_time_ticks(const base::TimeTicks& time_ticks) {
    network_portal_detector()->set_time_ticks_for_testing(time_ticks);
  }

  void advance_time_ticks(const base::TimeDelta& delta) {
    network_portal_detector()->advance_time_ticks_for_testing(delta);
  }

  void SetBehindPortal(const std::string& service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->SetProperty(
        dbus::ObjectPath(service_path), shill::kStateProperty,
        base::Value(shill::kStateNoConnectivity), base::DoNothing(),
        base::Bind(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  void SetNetworkDeviceEnabled(const std::string& type, bool enabled) {
    NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
        NetworkTypePattern::Primitive(type),
        enabled,
        network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetConnected(const std::string& service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->Connect(
        dbus::ObjectPath(service_path), base::DoNothing(),
        base::Bind(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  void SetDisconnected(const std::string& service_path) {
    DBusThreadManager::Get()->GetShillServiceClient()->Disconnect(
        dbus::ObjectPath(service_path), base::DoNothing(),
        base::Bind(&ErrorCallbackFunction));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<EnumHistogramChecker> MakeResultHistogramChecker() {
    return std::unique_ptr<EnumHistogramChecker>(new EnumHistogramChecker(
        "CaptivePortal.OOBE.DetectionResult",
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT,
        original_samples_.get()));
  }

 private:
  void AddService(const std::string& network_id,
                  const std::string& type) {
    DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface()->
        AddService(network_id /* service_path */,
                   network_id /* guid */,
                   network_id /* name */,
                   type,
                   shill::kStateIdle,
                   true /* add_to_visible */);
  }

  void SetupDefaultShillState() {
    base::RunLoop().RunUntilIdle();
    DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface()->
        ClearServices();
    AddService(kStubEthernet, shill::kTypeEthernet);
    AddService(kStubWireless1, shill::kTypeWifi);
    AddService(kStubWireless2, shill::kTypeWifi);
    AddService(kStubCellular, shill::kTypeCellular);
  }

  void SetupNetworkHandler() {
    SetupDefaultShillState();
    NetworkHandler::Initialize();
  }

  content::BrowserTaskEnvironment task_environment_;
  Profile* profile_ = nullptr;
  std::unique_ptr<NetworkPortalDetectorImpl> network_portal_detector_;
  std::unique_ptr<base::HistogramSamples> original_samples_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  TestingProfileManager test_profile_manager_;
};

TEST_F(NetworkPortalDetectorImplTest, NoPortal) {
  ASSERT_EQ(State::STATE_IDLE, state());

  SetConnected(kStubWireless1);

  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  CompleteURLFetch(net::OK, 204, nullptr);

  EXPECT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);
  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, Portal) {
  ASSERT_EQ(State::STATE_IDLE, state());

  // Check HTTP 200 response code.
  SetConnected(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 200, nullptr);

  EXPECT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  // Check HTTP 301 response code.
  SetConnected(kStubWireless2);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 301, nullptr);

  EXPECT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 301, kStubWireless2);

  // Check HTTP 302 response code.
  SetConnected(kStubEthernet);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 302, nullptr);

  EXPECT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 302, kStubEthernet);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 3)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, Online2Offline) {
  ASSERT_EQ(State::STATE_IDLE, state());

  MockObserver observer;
  AddObserver(&observer);

  NetworkPortalDetector::CaptivePortalState offline_state;
  offline_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE;

  // WiFi is in online state.
  {
    // When transitioning to a connected state, the network will transition to
    // connecting states which will set the default network to nullptr. This may
    // get triggered multiple times.
    EXPECT_CALL(observer, OnPortalDetectionCompleted(_, offline_state))
        .Times(AnyNumber());

    // Expect a single transition to an online state.
    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;
    EXPECT_CALL(observer, OnPortalDetectionCompleted(_, online_state)).Times(1);

    SetConnected(kStubWireless1);
    ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

    CompleteURLFetch(net::OK, 204, nullptr);
    EXPECT_NE(State::STATE_IDLE, state());

    // Check that observer was notified about online state.
    Mock::VerifyAndClearExpectations(&observer);
  }

  // WiFi is turned off.
  {
    EXPECT_CALL(observer, OnPortalDetectionCompleted(nullptr, offline_state))
        .Times(1);

    SetDisconnected(kStubWireless1);
    ASSERT_EQ(State::STATE_IDLE, state());

    // Check that observer was notified about offline state.
    Mock::VerifyAndClearExpectations(&observer);
  }

  RemoveObserver(&observer);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, TwoNetworks) {
  ASSERT_EQ(State::STATE_IDLE, state());

  SetConnected(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // WiFi is in portal state.
  CompleteURLFetch(net::OK, 200, nullptr);
  EXPECT_NE(State::STATE_IDLE, state());

  SetConnected(kStubEthernet);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubEthernet);
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, NetworkChanged) {
  ASSERT_EQ(State::STATE_IDLE, state());

  SetConnected(kStubWireless1);

  // WiFi is in portal state.
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // Active network is changed during portal detection for WiFi.
  SetConnected(kStubEthernet);

  // Portal detection for WiFi is cancelled, portal detection for
  // ethernet is initiated.
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // ethernet is in online state.
  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubEthernet);

  // As active network was changed during portal detection for wifi
  // network, it's state must be unknown.
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, NetworkStateNotChanged) {
  ASSERT_EQ(State::STATE_IDLE, state());

  SetConnected(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 204, nullptr);

  EXPECT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  SetConnected(kStubWireless1);
  ASSERT_EQ(State::STATE_IDLE, state());

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, NetworkStateChanged) {
  // Test for Portal -> Online -> Portal network state transitions.
  ASSERT_EQ(State::STATE_IDLE, state());

  SetBehindPortal(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 200, nullptr);

  ASSERT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  SetConnected(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 204, nullptr);

  EXPECT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  SetBehindPortal(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::OK, 200, nullptr);

  ASSERT_NE(State::STATE_IDLE, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 2)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionTimeout) {
  ASSERT_EQ(State::STATE_IDLE, state());

  // For instantaneous timeout.
  set_next_attempt_timeout(base::TimeDelta::FromSeconds(0));

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, same_detection_result_count());
  ASSERT_EQ(0, no_response_result_count());

  SetConnected(kStubWireless1);
  base::RunLoop().RunUntilIdle();

  // First portal detection timeouts, next portal detection is
  // scheduled.
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  ASSERT_EQ(1, no_response_result_count());

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectionRetryAfter) {
  ASSERT_EQ(State::STATE_IDLE, state());

  const char retry_after[] = "HTTP/1.1 503 OK\nRetry-After: 101\n\n";

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, no_response_result_count());

  SetConnected(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
  CompleteURLFetch(net::OK, 503, retry_after);

  // First portal detection completed, next portal detection is
  // scheduled after 101 seconds.
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  ASSERT_EQ(1, no_response_result_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(101), next_attempt_delay());

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, PortalDetectorRetryAfterIsSmall) {
  ASSERT_EQ(State::STATE_IDLE, state());

  const char retry_after[] = "HTTP/1.1 503 OK\nRetry-After: 1\n\n";

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, no_response_result_count());

  SetConnected(kStubWireless1);
  CompleteURLFetch(net::OK, 503, retry_after);

  // First portal detection completed, next portal detection is
  // scheduled after 3 seconds (due to minimum time between detection
  // attemps).
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  ASSERT_EQ(1, no_response_result_count());

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, FirstAttemptFailed) {
  ASSERT_EQ(State::STATE_IDLE, state());

  set_delay_till_next_attempt(base::TimeDelta());
  const char retry_after[] = "HTTP/1.1 503 OK\nRetry-After: 0\n\n";

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, no_response_result_count());

  SetConnected(kStubWireless1);

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  ASSERT_EQ(1, no_response_result_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 204, nullptr);
  EXPECT_NE(State::STATE_IDLE, state());
  ASSERT_EQ(0, no_response_result_count());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, AllAttemptsFailed) {
  ASSERT_EQ(State::STATE_IDLE, state());

  set_delay_till_next_attempt(base::TimeDelta());
  const char retry_after[] = "HTTP/1.1 503 OK\nRetry-After: 0\n\n";

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_EQ(0, no_response_result_count());

  SetConnected(kStubWireless1);

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  ASSERT_EQ(1, no_response_result_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_after);
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  ASSERT_EQ(2, no_response_result_count());
  ASSERT_EQ(base::TimeDelta::FromSeconds(0), next_attempt_delay());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 503, retry_after);
  EXPECT_NE(State::STATE_IDLE, state());
  ASSERT_EQ(3, no_response_result_count());
  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE,
                   503,
                   kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, ProxyAuthRequired) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());

  SetConnected(kStubWireless1);
  CompleteURLFetch(net::OK, 407, nullptr);
  ASSERT_EQ(1, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  base::RunLoop().RunUntilIdle();
  CompleteURLFetch(net::OK, 407, nullptr);
  ASSERT_EQ(2, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  base::RunLoop().RunUntilIdle();
  CompleteURLFetch(net::OK, 407, nullptr);
  ASSERT_EQ(3, no_response_result_count());
  EXPECT_NE(State::STATE_IDLE, state());

  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED,
      407,
      kStubWireless1);

  ASSERT_TRUE(MakeResultHistogramChecker()
                  ->Expect(NetworkPortalDetector::
                               CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED,
                           1)
                  ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, NoResponseButBehindPortal) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());

  SetBehindPortal(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED, 0, nullptr);
  ASSERT_EQ(1, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED, 0, nullptr);
  ASSERT_EQ(2, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED, 0, nullptr);
  ASSERT_EQ(3, no_response_result_count());
  EXPECT_NE(State::STATE_IDLE, state());

  CheckPortalState(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 0,
                   kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest,
       DisableErrorScreenStrategyWhilePendingRequest) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_no_response_result_count(3);
  enable_error_screen_strategy();
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  disable_error_screen_strategy();

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, ErrorScreenStrategyForOnlineNetwork) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());

  SetConnected(kStubWireless1);
  enable_error_screen_strategy();
  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();
  CompleteURLFetch(net::OK, 204, nullptr);

  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 204, nullptr);

  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  disable_error_screen_strategy();

  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
  CompleteURLFetch(net::OK, 204, nullptr);

  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, ErrorScreenStrategyForPortalNetwork) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());

  enable_error_screen_strategy();
  SetConnected(kStubWireless1);

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED, 0, nullptr);
  ASSERT_EQ(1, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED, 0, nullptr);
  ASSERT_EQ(2, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  CompleteURLFetch(net::OK, 200, nullptr);
  ASSERT_EQ(0, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  disable_error_screen_strategy();

  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, DetectionTimeoutIsCancelled) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());

  SetConnected(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  stop_detection();

  ASSERT_EQ(State::STATE_IDLE, state());
  ASSERT_TRUE(attempt_timeout_is_cancelled());
  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN, -1, kStubWireless1);

  ASSERT_TRUE(MakeResultHistogramChecker()->Check());
}

TEST_F(NetworkPortalDetectorImplTest, TestDetectionRestart) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());

  // First portal detection attempts determines ONLINE state.
  SetConnected(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
  ASSERT_FALSE(StartPortalDetection(false /* force */));

  CompleteURLFetch(net::OK, 204, nullptr);

  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204, kStubWireless1);
  EXPECT_NE(State::STATE_IDLE, state());

  // First portal detection attempts determines PORTAL state.
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());
  CompleteURLFetch(net::OK, 200, nullptr);

  CheckPortalState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 200, kStubWireless1);
  EXPECT_NE(State::STATE_IDLE, state());

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, RequestTimeouts) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());

  SetNetworkDeviceEnabled(shill::kTypeWifi, false);
  SetConnected(kStubCellular);

  // First portal detection attempt for cellular1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(
      0 /* expected_same_detection_result_count */,
      0 /* expected_no_response_result_count */,
      5 /* expected_request_timeout_sec */, net::ERR_CONNECTION_CLOSED, 0);

  // Second portal detection attempt for cellular1 uses 10sec timeout.
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(
      1 /* expected_same_detection_result_count */,
      1 /* expected_no_response_result_count */,
      10 /* expected_request_timeout_sec */, net::ERR_CONNECTION_CLOSED, 0);

  // Third portal detection attempt for cellular1 uses 15sec timeout.
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(
      2 /* expected_same_detection_result_count */,
      2 /* expected_no_response_result_count */,
      15 /* expected_request_timeout_sec */, net::ERR_CONNECTION_CLOSED, 0);

  EXPECT_NE(State::STATE_IDLE, state());

  // Check that on the error screen 15sec timeout is used.
  enable_error_screen_strategy();
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(
      0 /* expected_same_detection_result_count */,
      0 /* expected_no_response_result_count */,
      15 /* expected_request_timeout_sec */, net::ERR_CONNECTION_CLOSED, 0);
  disable_error_screen_strategy();
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  SetNetworkDeviceEnabled(shill::kTypeWifi, true);
  SetConnected(kStubWireless1);

  // First portal detection attempt for wifi1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(
      0 /* expected_same_detection_result_count */,
      0 /* expected_no_response_result_count */,
      5 /* expected_request_timeout_sec */, net::ERR_CONNECTION_CLOSED, 0);

  // Second portal detection attempt for wifi1 also uses 5sec timeout.
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(
      1 /* expected_same_detection_result_count */,
      1 /* expected_no_response_result_count */,
      10 /* expected_request_timeout_sec */,
      net::OK,
      204);
  EXPECT_NE(State::STATE_IDLE, state());

  // Check that in error screen strategy detection for wifi1 15sec
  // timeout is used.
  enable_error_screen_strategy();
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(
      0 /* expected_same_detection_result_count */,
      0 /* expected_no_response_result_count */,
      15 /* expected_request_timeout_sec */,
      net::OK,
      204);
  disable_error_screen_strategy();
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

TEST_F(NetworkPortalDetectorImplTest, RequestTimeouts2) {
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());
  SetConnected(kStubWireless1);

  // First portal detection attempt for wifi1 uses 5sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(
      0 /* expected_same_detection_result_count */,
      0 /* expected_no_response_result_count */,
      5 /* expected_request_timeout_sec */, net::ERR_CONNECTION_CLOSED, 0);
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  base::RunLoop().RunUntilIdle();

  // Second portal detection attempt for wifi1 uses 10sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(
      1 /* expected_same_detection_result_count */,
      1 /* expected_no_response_result_count */,
      10 /* expected_request_timeout_sec */, net::ERR_CONNECTION_CLOSED, 0);
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());
  base::RunLoop().RunUntilIdle();

  // Second portal detection attempt for wifi1 uses 15sec timeout.
  CheckRequestTimeoutAndCompleteAttempt(
      2 /* expected_same_detection_result_count */,
      2 /* expected_no_response_result_count */,
      15 /* expected_request_timeout_sec */, net::ERR_CONNECTION_CLOSED, 0);
  EXPECT_NE(State::STATE_IDLE, state());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  // Third portal detection attempt for wifi1 uses 20sec timeout.
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(
      3 /* expected_same_detection_result_count */,
      3 /* expected_no_response_result_count */,
      20 /* expected_request_timeout_sec */,
      net::OK,
      204);
  EXPECT_NE(State::STATE_IDLE, state());

  // Fourth portal detection attempt for wifi1 uses 5sec timeout.
  base::RunLoop().RunUntilIdle();
  CheckRequestTimeoutAndCompleteAttempt(
      1 /* expected_same_detection_result_count */,
      0 /* expected_no_response_result_count */,
      5 /* expected_request_timeout_sec */,
      net::OK,
      204);
  EXPECT_NE(State::STATE_IDLE, state());

  ASSERT_TRUE(
      MakeResultHistogramChecker()
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE, 1)
          ->Expect(NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 1)
          ->Check());
}

// The randomized alternate hosts for captive portal detection is deployed but
// we are curious what is the effect (crbug.com/742437).
// Tests that UMA records correctly for the case that after shill reports portal
// we may get blacklisted.
TEST_F(NetworkPortalDetectorImplTest, BehindPortalAndThenBlacklisted) {
  base::HistogramTester histograms_;
  ASSERT_EQ(State::STATE_IDLE, state());
  set_delay_till_next_attempt(base::TimeDelta());

  // Shill reports portal network.
  SetBehindPortal(kStubWireless1);
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  // Then we get blacklisted, each URL fetch may give us no response result.
  advance_time_ticks(NetworkPortalDetectorImpl::kDelaySinceShillPortalForUMA -
                     base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(State::STATE_CHECKING_FOR_PORTAL, state());

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED, 0, nullptr);
  ASSERT_EQ(1, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  histograms_.ExpectBucketCount("CaptivePortal.DetectionResultSincePortal",
                                true, 1);

  // Verifies that the offline result is not recorded after
  // kDelaySincePortalNetworkForUMA.
  advance_time_ticks(base::TimeDelta::FromSeconds(2));

  CompleteURLFetch(net::ERR_CONNECTION_CLOSED, 0, nullptr);
  ASSERT_EQ(2, no_response_result_count());
  ASSERT_EQ(State::STATE_PORTAL_CHECK_PENDING, state());

  // To run CaptivePortalDetector::DetectCaptivePortal().
  base::RunLoop().RunUntilIdle();

  histograms_.ExpectBucketCount("CaptivePortal.DetectionResultSincePortal",
                                true, 1);
}

}  // namespace chromeos
