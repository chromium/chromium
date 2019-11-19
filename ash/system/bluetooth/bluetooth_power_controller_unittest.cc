// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_power_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"

namespace ash {
namespace {

constexpr char kUser1Email[] = "user1@bluetooth";
constexpr bool kUserFirstLogin = true;

void SetupBluetoothAdapter() {
  // Set Bluetooth discovery simulation delay to 0 so the test doesn't have to
  // wait or use timers.
  bluez::FakeBluetoothAdapterClient* adapter_client =
      static_cast<bluez::FakeBluetoothAdapterClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient());
  adapter_client->SetSimulationIntervalMs(0);

  // Makes sure we get the callback from BluetoothAdapterFactory::GetAdapter
  // first before running the remaining test.
  base::RunLoop().RunUntilIdle();
}

BluetoothPowerController* GetController() {
  return Shell::Get()->bluetooth_power_controller();
}

device::BluetoothAdapter* GetBluetoothAdapter() {
  return GetController()->bluetooth_adapter_for_test();
}

}  // namespace

// NOTE: Manually controls local state prefs and user prefs so to allow tests
// of default pref values before the pref service initialization notifications.
class BluetoothPowerControllerTest : public NoSessionAshTestBase {
 public:
  BluetoothPowerControllerTest() { disable_provide_local_state(); }
  ~BluetoothPowerControllerTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    BluetoothPowerController::RegisterProfilePrefs(
        active_user_prefs_.registry());
    BluetoothPowerController::RegisterLocalStatePrefs(local_state_.registry());

    GetController()->local_state_ = &local_state_;

    SetupBluetoothAdapter();
  }

  void AddUserSessionAndStartWatchingPrefsChanges(
      const std::string& display_email,
      user_manager::UserType user_type = user_manager::USER_TYPE_REGULAR,
      bool is_new_profile = false) {
    GetSessionControllerClient()->AddUserSession(
        display_email, user_type, false /* enable_settings */,
        false /* provide_pref_service */, is_new_profile);
    GetController()->active_user_pref_service_ = &active_user_prefs_;
    GetController()->StartWatchingActiveUserPrefsChanges();
  }

  void StartWatchingLocalStatePrefsChanges() {
    GetController()->StartWatchingLocalStatePrefsChanges();
  }

 protected:
  void ApplyBluetoothLocalStatePref() {
    GetController()->ApplyBluetoothLocalStatePref();
  }

  void ApplyBluetoothPrimaryUserPref() {
    GetController()->ApplyBluetoothPrimaryUserPref();
  }

  const base::queue<BluetoothPowerController::BluetoothTask>&
  GetPendingBluetoothTasks() {
    return GetController()->pending_bluetooth_tasks_;
  }

  // Pretends that the controller is busy doing bluetooth work. This is needed
  // to test the behavior when multiple power change requests are queued when
  // the controller is busy.
  void SimulateControllerBusy(bool is_busy) {
    GetController()->pending_tasks_busy_ = is_busy;
    if (!is_busy)
      GetController()->TriggerRunPendingBluetoothTasks();
  }

  TestingPrefServiceSimple active_user_prefs_;
  TestingPrefServiceSimple local_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothPowerControllerTest);
};

// Tests toggling Bluetooth setting on and off.
TEST_F(BluetoothPowerControllerTest, ToggleBluetoothEnabled) {
  // Toggling bluetooth on/off when there is no user session should affect
  // local state prefs.
  EXPECT_FALSE(local_state_.GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  GetController()->SetBluetoothEnabled(true);
  EXPECT_TRUE(local_state_.GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  GetController()->SetBluetoothEnabled(false);
  EXPECT_FALSE(local_state_.GetBoolean(prefs::kSystemBluetoothAdapterEnabled));

  // Toggling bluetooth on/off when there is user session should affect
  // user prefs.
  AddUserSessionAndStartWatchingPrefsChanges(kUser1Email);
  EXPECT_FALSE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  GetController()->SetBluetoothEnabled(true);
  EXPECT_TRUE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  GetController()->SetBluetoothEnabled(false);
  EXPECT_FALSE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));
}

// Tests that BluetoothPowerController listens to local state pref changes
// and applies the changes to bluetooth device.
TEST_F(BluetoothPowerControllerTest, ListensPrefChangesLocalState) {
  StartWatchingLocalStatePrefsChanges();

  // Makes sure we start with bluetooth power off.
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());
  EXPECT_FALSE(local_state_.GetBoolean(prefs::kSystemBluetoothAdapterEnabled));

  // Power should be turned on when pref changes to enabled.
  local_state_.SetBoolean(prefs::kSystemBluetoothAdapterEnabled, true);
  EXPECT_TRUE(GetBluetoothAdapter()->IsPowered());

  // Power should be turned off when pref changes to disabled.
  local_state_.SetBoolean(prefs::kSystemBluetoothAdapterEnabled, false);
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());
}

// Tests that BluetoothPowerController listens to active user pref changes
// and applies the changes to bluetooth device.
TEST_F(BluetoothPowerControllerTest, ListensPrefChangesActiveUser) {
  AddUserSessionAndStartWatchingPrefsChanges(kUser1Email);

  // Makes sure we start with bluetooth power off.
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());
  EXPECT_FALSE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));

  // Power should be turned on when pref changes to enabled.
  active_user_prefs_.SetBoolean(prefs::kUserBluetoothAdapterEnabled, true);
  EXPECT_TRUE(GetBluetoothAdapter()->IsPowered());

  // Power should be turned off when pref changes to disabled.
  active_user_prefs_.SetBoolean(prefs::kUserBluetoothAdapterEnabled, false);
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());
}

// Tests that BluetoothPowerController listens to multiple active user pref
// changes and applies the changes to bluetooth device. The queued multiple
// power change tasks shouldn't be executed all but rather only the last request
// is executed.
TEST_F(BluetoothPowerControllerTest, ListensPrefChangesLongQueue) {
  AddUserSessionAndStartWatchingPrefsChanges(kUser1Email);

  // Makes sure we start with bluetooth power off.
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());
  EXPECT_FALSE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));

  // Multiple power change requests come in when the controller is busy.
  SimulateControllerBusy(true);
  active_user_prefs_.SetBoolean(prefs::kUserBluetoothAdapterEnabled, true);
  active_user_prefs_.SetBoolean(prefs::kUserBluetoothAdapterEnabled, false);
  active_user_prefs_.SetBoolean(prefs::kUserBluetoothAdapterEnabled, true);
  active_user_prefs_.SetBoolean(prefs::kUserBluetoothAdapterEnabled, false);
  active_user_prefs_.SetBoolean(prefs::kUserBluetoothAdapterEnabled, true);

  // The controller should execute only the last request.
  EXPECT_EQ(1u, GetPendingBluetoothTasks().size());
  // Flush the queue to be executed.
  SimulateControllerBusy(false);
  // The power state should represent the last request in the queue.
  EXPECT_TRUE(GetBluetoothAdapter()->IsPowered());
}

// Tests how BluetoothPowerController applies the local state pref when
// the pref hasn't been set before.
TEST_F(BluetoothPowerControllerTest, ApplyBluetoothLocalStatePrefDefault) {
  // Makes sure pref hasn't been set before.
  EXPECT_TRUE(
      local_state_.FindPreference(prefs::kSystemBluetoothAdapterEnabled)
          ->IsDefaultValue());
  // Start with bluetooth power on.
  GetBluetoothAdapter()->SetPowered(true, base::DoNothing(), base::DoNothing());
  EXPECT_TRUE(GetBluetoothAdapter()->IsPowered());

  ApplyBluetoothLocalStatePref();

  // Pref should now contain the current bluetooth adapter state (on).
  EXPECT_FALSE(
      local_state_.FindPreference(prefs::kSystemBluetoothAdapterEnabled)
          ->IsDefaultValue());
  EXPECT_TRUE(local_state_.GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
}

// Tests how BluetoothPowerController applies the local state pref when
// the pref has been set before.
TEST_F(BluetoothPowerControllerTest, ApplyBluetoothLocalStatePrefOn) {
  // Set the pref to true.
  local_state_.SetBoolean(prefs::kSystemBluetoothAdapterEnabled, true);
  EXPECT_FALSE(
      local_state_.FindPreference(prefs::kSystemBluetoothAdapterEnabled)
          ->IsDefaultValue());
  // Start with bluetooth power off.
  GetBluetoothAdapter()->SetPowered(false, base::DoNothing(),
                                    base::DoNothing());
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());

  ApplyBluetoothLocalStatePref();

  // Bluetooth power setting should be applied (on), and pref value unchanged.
  EXPECT_TRUE(local_state_.GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_TRUE(GetBluetoothAdapter()->IsPowered());
}

// Tests how BluetoothPowerController applies the user pref when
// the pref hasn't been set before.
TEST_F(BluetoothPowerControllerTest, ApplyBluetoothPrimaryUserPrefDefault) {
  AddUserSessionAndStartWatchingPrefsChanges(kUser1Email);

  // Makes sure pref hasn't been set before.
  EXPECT_TRUE(
      active_user_prefs_.FindPreference(prefs::kUserBluetoothAdapterEnabled)
          ->IsDefaultValue());
  // Start with bluetooth power off.
  GetBluetoothAdapter()->SetPowered(false, base::DoNothing(),
                                    base::DoNothing());
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());

  ApplyBluetoothPrimaryUserPref();

  // Pref should now contain the current bluetooth adapter state (off).
  EXPECT_FALSE(
      active_user_prefs_.FindPreference(prefs::kUserBluetoothAdapterEnabled)
          ->IsDefaultValue());
  EXPECT_FALSE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));
}

// Tests how BluetoothPowerController applies the user pref when
// the pref hasn't been set before, and it's a first-login user.
TEST_F(BluetoothPowerControllerTest, ApplyBluetoothPrimaryUserPrefDefaultNew) {
  AddUserSessionAndStartWatchingPrefsChanges(
      kUser1Email, user_manager::USER_TYPE_REGULAR, kUserFirstLogin);

  // Makes sure pref hasn't been set before.
  EXPECT_TRUE(
      active_user_prefs_.FindPreference(prefs::kUserBluetoothAdapterEnabled)
          ->IsDefaultValue());
  // Start with bluetooth power off.
  GetBluetoothAdapter()->SetPowered(false, base::DoNothing(),
                                    base::DoNothing());
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());

  ApplyBluetoothPrimaryUserPref();

  // Pref should be set to true for first-login users, and this will also
  // trigger the bluetooth power on.
  EXPECT_FALSE(
      active_user_prefs_.FindPreference(prefs::kUserBluetoothAdapterEnabled)
          ->IsDefaultValue());
  EXPECT_TRUE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_TRUE(GetBluetoothAdapter()->IsPowered());
}

// Tests how BluetoothPowerController applies the user pref when
// the pref hasn't been set before, but not a regular user (e.g. kiosk).
TEST_F(BluetoothPowerControllerTest, ApplyBluetoothKioskUserPrefDefault) {
  AddUserSessionAndStartWatchingPrefsChanges(kUser1Email,
                                             user_manager::USER_TYPE_KIOSK_APP);

  // Makes sure pref hasn't been set before.
  EXPECT_TRUE(
      active_user_prefs_.FindPreference(prefs::kUserBluetoothAdapterEnabled)
          ->IsDefaultValue());
  // Start with bluetooth power off.
  GetBluetoothAdapter()->SetPowered(false, base::DoNothing(),
                                    base::DoNothing());
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());

  ApplyBluetoothPrimaryUserPref();

  // For non-regular user, do not apply the bluetooth setting and no need
  // to set the pref.
  EXPECT_TRUE(
      active_user_prefs_.FindPreference(prefs::kUserBluetoothAdapterEnabled)
          ->IsDefaultValue());
  EXPECT_FALSE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());
}

// Tests how BluetoothPowerController applies the user pref when
// the pref has been set before.
TEST_F(BluetoothPowerControllerTest, ApplyBluetoothPrimaryUserPrefOn) {
  AddUserSessionAndStartWatchingPrefsChanges(kUser1Email);

  // Set the pref to true.
  active_user_prefs_.SetBoolean(prefs::kUserBluetoothAdapterEnabled, true);
  EXPECT_FALSE(
      active_user_prefs_.FindPreference(prefs::kUserBluetoothAdapterEnabled)
          ->IsDefaultValue());
  // Start with bluetooth power off.
  GetBluetoothAdapter()->SetPowered(false, base::DoNothing(),
                                    base::DoNothing());
  EXPECT_FALSE(GetBluetoothAdapter()->IsPowered());

  ApplyBluetoothPrimaryUserPref();

  // Pref should be applied to trigger the bluetooth power on, and the pref
  // value should be unchanged..
  EXPECT_TRUE(
      active_user_prefs_.GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_TRUE(GetBluetoothAdapter()->IsPowered());
}

using BluetoothPowerControllerIntegrationTest = NoSessionAshTestBase;

// An "integration" test using the PrefService objects provided by AshTestBase
// and TestSessionControllerClient to provide a closer simulation of production
// login behavior. http://crbug.com/762567
TEST_F(BluetoothPowerControllerIntegrationTest, Basics) {
  SetupBluetoothAdapter();
  device::BluetoothAdapter* adapter = GetBluetoothAdapter();

  // Verify toggling bluetooth before login.
  PrefService* local_state = ash_test_helper()->GetLocalStatePrefService();
  GetController()->SetBluetoothEnabled(true);
  EXPECT_TRUE(local_state->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_TRUE(adapter->IsPowered());
  GetController()->SetBluetoothEnabled(false);
  EXPECT_FALSE(local_state->GetBoolean(prefs::kSystemBluetoothAdapterEnabled));
  EXPECT_FALSE(adapter->IsPowered());

  // Verify toggling bluetooth after login.
  SimulateUserLogin(kUser1Email);
  PrefService* user_prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  GetController()->SetBluetoothEnabled(true);
  EXPECT_TRUE(user_prefs->GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_TRUE(adapter->IsPowered());
  GetController()->SetBluetoothEnabled(false);
  EXPECT_FALSE(user_prefs->GetBoolean(prefs::kUserBluetoothAdapterEnabled));
  EXPECT_FALSE(adapter->IsPowered());
}

}  // namespace ash
