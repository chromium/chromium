// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_debug_manager_client.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/fake_floss_adapter_client.h"
#include "device/bluetooth/floss/fake_floss_logging_client.h"
#include "device/bluetooth/floss/fake_floss_manager_client.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/floss/floss_manager_client.h"
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

    is_floss_flag_enabled_ = false;
  }

  void TearDown() override {
    debug_logs_manager_.reset();
    adapter_.reset();
    bluez::BluezDBusManager::Shutdown();
  }

  void InitFlossFakes() {
    std::unique_ptr<floss::FlossDBusManagerSetter> floss_setter =
        floss::FlossDBusManager::GetSetterForTesting();
    floss_setter->SetFlossManagerClient(
        std::make_unique<floss::FakeFlossManagerClient>());
    floss_setter->SetFlossAdapterClient(
        std::make_unique<floss::FakeFlossAdapterClient>());
    floss_setter->SetFlossLoggingClient(
        std::make_unique<floss::FakeFlossLoggingClient>());

    GetFakeManagerClient()->SetDefaultEnabled(true);
  }

  void EnableDebugFlag() { is_debug_toggle_flag_enabled_ = true; }
  void EnableFlossFlag() { is_floss_flag_enabled_ = true; }

  void InitFeatures() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (is_debug_toggle_flag_enabled_)
      enabled_features.push_back(features::kShowBluetoothDebugLogToggle);
    else
      disabled_features.push_back(features::kShowBluetoothDebugLogToggle);

    if (is_floss_flag_enabled_) {
      enabled_features.push_back(floss::features::kFlossEnabled);
    } else {
      disabled_features.push_back(floss::features::kFlossEnabled);
    }

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

  raw_ptr<floss::FakeFlossManagerClient> GetFakeManagerClient() const {
    return static_cast<floss::FakeFlossManagerClient*>(
        floss::FlossDBusManager::Get()->GetManagerClient());
  }

  raw_ptr<floss::FakeFlossLoggingClient> GetFakeFlossLoggingClient() const {
    return static_cast<floss::FakeFlossLoggingClient*>(
        floss::FlossDBusManager::Get()->GetLoggingClient());
  }

  void InitializeAdapter(bool powered) {
    adapter_ = floss::BluetoothAdapterFloss::CreateAdapter();

    base::RunLoop run_loop;
    adapter_->Initialize(run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_TRUE(adapter_);
    ASSERT_TRUE(adapter_->IsInitialized());

    device::BluetoothAdapterFactory::Get()->SetAdapterForTesting(adapter_);
  }

  void SimulatePowered(bool powered) {
    adapter_->NotifyAdapterPoweredChanged(powered);
  }

  bool IsDebugEnabled() {
    return debug_logs_manager_->GetDebugLogsState() ==
           DebugLogsManager::DebugLogsState::kSupportedAndEnabled;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  bool is_debug_toggle_flag_enabled_ = false;
  bool is_floss_flag_enabled_ = false;
  raw_ptr<bluez::FakeBluetoothDebugManagerClient, DanglingUntriaged>
      fake_bluetooth_debug_manager_client_;
  std::unique_ptr<DebugLogsManager> debug_logs_manager_;
  TestingPrefServiceSimple prefs_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
};

TEST_F(DebugLogsManagerTest, FlagNotEnabled) {
  base::test::SingleThreadTaskEnvironment task_environment;

  /* debug flag disabled */
  InitFeatures();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kNotSupported);
}

TEST_F(DebugLogsManagerTest, NonGoogler) {
  base::test::SingleThreadTaskEnvironment task_environment;

  EnableDebugFlag();
  InitFeatures();
  InstantiateDebugManager(kTestNonGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kNotSupported);
}

TEST_F(DebugLogsManagerTest, GooglerDefaultPref) {
  base::test::SingleThreadTaskEnvironment task_environment;

  EnableDebugFlag();
  InitFeatures();
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(debug_manager()->GetDebugLogsState(),
            DebugLogsManager::DebugLogsState::kSupportedAndEnabled);
}

TEST_F(DebugLogsManagerTest, ChangeDebugLogsState) {
  base::test::SingleThreadTaskEnvironment task_environment;

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
  base::test::SingleThreadTaskEnvironment task_environment;

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

TEST_F(DebugLogsManagerTest, CheckFlossUpdatesOnPowerOn) {
  base::test::SingleThreadTaskEnvironment task_environment;

  InitFlossFakes();
  InitializeAdapter(/*powered=*/false);

  EnableDebugFlag();
  EnableFlossFlag();
  InitFeatures();

  // Until we're powered, setting debug logging should fail but the default
  // state should be persisted.
  EXPECT_EQ(GetFakeFlossLoggingClient()->GetDebugEnabledForTesting(), false);
  InstantiateDebugManager(kTestGooglerEmail);
  EXPECT_EQ(GetFakeFlossLoggingClient()->GetDebugEnabledForTesting(), false);
  EXPECT_EQ(IsDebugEnabled(), true);

  // Powering on should enable the flag.
  SimulatePowered(/*powered=*/true);
  EXPECT_EQ(GetFakeFlossLoggingClient()->GetDebugEnabledForTesting(), true);
  SimulatePowered(/*powered=*/false);
}

}  // namespace bluetooth

}  // namespace ash
