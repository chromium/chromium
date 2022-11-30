// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_debug_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace bluetooth {

namespace {

constexpr char kTestGooglerEmail[] = "user@google.com";
constexpr char kTestNonGooglerEmail[] = "user@gmail.com";

}  // namespace

class DebugLogsManagerTest : public testing::Test {
 public:
  DebugLogsManagerTest() = default;

  DebugLogsManagerTest(const DebugLogsManagerTest&) = delete;
  DebugLogsManagerTest& operator=(const DebugLogsManagerTest&) = delete;

  void SetUp() override {
    DebugLogsManager::RegisterPrefs(prefs_.registry());

    auto fake_bluetooth_debug_manager_client =
        std::make_unique<bluez::FakeBluetoothDebugManagerClient>();
    fake_bluetooth_debug_manager_client_ =
        fake_bluetooth_debug_manager_client.get();

    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();
    dbus_setter->SetBluetoothDebugManagerClient(
        std::unique_ptr<bluez::BluetoothDebugManagerClient>(
            std::move(fake_bluetooth_debug_manager_client)));
  }

  void TearDown() override {
    debug_logs_manager_.reset();
    bluez::BluezDBusManager::Shutdown();
  }

  void EnableDebugFlag() { is_debug_toggle_flag_enabled_ = true; }

  void InitFeatures() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (is_debug_toggle_flag_enabled_)
      enabled_features.push_back(features::kShowBluetoothDebugLogToggle);
    else
      disabled_features.push_back(features::kShowBluetoothDebugLogToggle);

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void InstantiateDebugManager(const char* email) {
    debug_logs_manager_ = std::make_unique<DebugLogsManager>(email, &prefs_);
  }

  void DestroyDebugManager() { debug_logs_manager_.reset(); }

  DebugLogsManager* debug_manager() const { return debug_logs_manager_.get(); }

  bluez::FakeBluetoothDebugManagerClient* fake_bluetooth_debug_manager_client()
      const {
    return fake_bluetooth_debug_manager_client_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  bool is_debug_toggle_flag_enabled_ = false;
  bluez::FakeBluetoothDebugManagerClient* fake_bluetooth_debug_manager_client_;
  std::unique_ptr<DebugLogsManager> debug_logs_manager_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(DebugLogsManagerTest, FlagNotEnabled) {
  /* debug flag disabled */
  InitFeatures();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kNotSupported);
}

TEST_F(DebugLogsManagerTest, NonGoogler) {
  EnableDebugFlag();
  InitFeatures();
  InstantiateDebugManager(kTestNonGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kNotSupported);
}

TEST_F(DebugLogsManagerTest, GooglerDefaultPref) {
  EnableDebugFlag();
  InitFeatures();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedAndEnabled);
}

TEST_F(DebugLogsManagerTest, ChangeDebugLogsState) {
  EnableDebugFlag();
  InitFeatures();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedAndEnabled);

  debug_manager()->ChangeDebugLogsState(
      false /* should_debug_logs_be_enabled */);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedButDisabled);

  DestroyDebugManager();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedButDisabled);
}

TEST_F(DebugLogsManagerTest, SendVerboseLogsRequestUponLoginAndLogout) {
  EnableDebugFlag();
  InitFeatures();
  InstantiateDebugManager(kTestGooglerEmail);
  // After login, bluez level should change
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->bluez_level(), 1);

  DestroyDebugManager();
  // After logout, bluez level should reset to 0
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->bluez_level(), 0);

  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->bluez_level(), 1);

  debug_manager()->ChangeDebugLogsState(
      false /* should_debug_logs_be_enabled */);
  // BlueZ level is updated only on login/logout, so now it stays the same.
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->bluez_level(), 1);
  DestroyDebugManager();
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->bluez_level(), 0);

  InstantiateDebugManager(kTestGooglerEmail);
  // bluez level should still be 0 because logging is disabled
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->bluez_level(), 0);
}

TEST_F(DebugLogsManagerTest, RetryUponSetVerboseLogsFailure) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  EnableDebugFlag();
  InitFeatures();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->bluez_level(), 1);
  debug_manager()->ChangeDebugLogsState(
      false /* should_debug_logs_be_enabled */);

  DestroyDebugManager();
  fake_bluetooth_debug_manager_client()->MakeNextSetLogLevelsFail();
  InstantiateDebugManager(kTestGooglerEmail);
  task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(fake_bluetooth_debug_manager_client()->set_log_levels_fail_count(),
            1);
  // Message is re-sent upon failing, eventually bluez level should change.
  EXPECT_EQ(fake_bluetooth_debug_manager_client()->bluez_level(), 0);
}

}  // namespace bluetooth

}  // namespace ash
