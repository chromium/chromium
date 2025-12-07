// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/wifi_signal_strength_rssi_fetcher.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::NetworkHandlerTestHelper;
using ::testing::Eq;
using ::testing::SizeIs;

namespace reporting {
namespace {

class WifiSignalStrengthRssiFetcherTest : public ::testing::Test {
 public:
  WifiSignalStrengthRssiFetcherTest() = default;

  WifiSignalStrengthRssiFetcherTest(const WifiSignalStrengthRssiFetcherTest&) =
      delete;
  WifiSignalStrengthRssiFetcherTest& operator=(
      const WifiSignalStrengthRssiFetcherTest&) = delete;

  ~WifiSignalStrengthRssiFetcherTest() override = default;

  void SetUp() override {
    // TODO(b/278643115) Remove LoginState dependency.
    ash::LoginState::Initialize();

    user_manager::UserManagerImpl::RegisterPrefs(local_state_.registry());
    fake_user_manager_.Reset(
        std::make_unique<user_manager::FakeUserManager>(&local_state_));
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@test", GaiaId("fakegaia"));
    fake_user_manager_->AddGaiaUser(account_id,
                                    user_manager::UserType::kRegular);
    fake_user_manager_->UserLoggedIn(account_id,
                                     network_handler_test_helper_.UserHash());

    ash::LoginState::Get()->SetLoggedInState(
        ash::LoginState::LOGGED_IN_ACTIVE,
        ash::LoginState::LOGGED_IN_USER_REGULAR);

    network_handler_test_helper_.AddDefaultProfiles();
    network_handler_test_helper_.ResetDevicesAndServices();
  }

  void TearDown() override {
    fake_user_manager_.Reset();
    ash::LoginState::Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;

  NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST_F(WifiSignalStrengthRssiFetcherTest, Default) {
  std::string service_path1 = network_handler_test_helper_.ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "ready",
            "WiFi.SignalStrengthRssi": -44})");
  std::string service_path2 = network_handler_test_helper_.ConfigureService(
      R"({"GUID": "wifi2_guid", "Type": "wifi", "State": "ready"})");
  std::string service_path3 = network_handler_test_helper_.ConfigureService(
      R"({"GUID": "wifi3_guid", "Type": "wifi", "State": "ready",
            "WiFi.SignalStrengthRssi": -70})");
  bool callback_called = false;

  base::RunLoop run_loop;
  FetchWifiSignalStrengthRssi(
      base::queue<std::string>({service_path1, service_path3}),
      base::BindLambdaForTesting([&](base::flat_map<std::string, int> result) {
        callback_called = true;

        EXPECT_THAT(result, SizeIs(2));
        ASSERT_TRUE(base::Contains(result, service_path1));
        EXPECT_THAT(result.at(service_path1), Eq(-44));
        ASSERT_TRUE(base::Contains(result, service_path3));
        EXPECT_THAT(result.at(service_path3), Eq(-70));

        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(callback_called);
}

TEST_F(WifiSignalStrengthRssiFetcherTest, OneServiceWithNoRssiValue) {
  std::string service_path1 = network_handler_test_helper_.ConfigureService(
      R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "online",
            "WiFi.SignalStrengthRssi": -44})");
  std::string service_path2 = network_handler_test_helper_.ConfigureService(
      R"({"GUID": "wifi2_guid", "Type": "wifi", "State": "ready"})");
  std::string service_path3 = network_handler_test_helper_.ConfigureService(
      R"({"GUID": "wifi3_guid", "Type": "wifi", "State": "ready",
            "WiFi.SignalStrengthRssi": -70})");
  bool callback_called = false;

  base::RunLoop run_loop;
  FetchWifiSignalStrengthRssi(
      base::queue<std::string>({service_path1, service_path2, service_path3}),
      base::BindLambdaForTesting([&](base::flat_map<std::string, int> result) {
        callback_called = true;

        EXPECT_THAT(result, SizeIs(2));
        ASSERT_TRUE(base::Contains(result, service_path1));
        EXPECT_THAT(result.at(service_path1), Eq(-44));
        ASSERT_TRUE(base::Contains(result, service_path3));
        EXPECT_THAT(result.at(service_path3), Eq(-70));

        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(callback_called);
}

TEST_F(WifiSignalStrengthRssiFetcherTest, EmptyInput) {
  bool callback_called = false;

  base::RunLoop run_loop;
  FetchWifiSignalStrengthRssi(
      {},
      base::BindLambdaForTesting([&](base::flat_map<std::string, int> result) {
        callback_called = true;
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(callback_called);
}

}  // namespace
}  // namespace reporting
