// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/hps/fake_hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"

namespace ash {

// Enables or disables the user pref for the feature.
void SetEnabledPref(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSnoopingProtectionEnabled, enabled);
}

// A fixture that provides access to a fake daemon and an instance of the
// controller hooked up to the test environment.
class HpsNotifyControllerTestBase : public NoSessionAshTestBase {
 public:
  // Argument controls whether the DBus method should return true or false when
  // the controller is being initialized. We can't set this value in individual
  // tests since it must be done before AshTestBase::SetUp() executes.
  HpsNotifyControllerTestBase(bool initial_dbus_state)
      : initial_dbus_state_(initial_dbus_state) {}
  HpsNotifyControllerTestBase(const HpsNotifyControllerTestBase&) = delete;
  HpsNotifyControllerTestBase& operator=(const HpsNotifyControllerTestBase&) =
      delete;
  ~HpsNotifyControllerTestBase() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kSnoopingProtection);

    chromeos::HpsDBusClient::InitializeFake();
    dbus_client_ = chromeos::FakeHpsDBusClient::Get();
    dbus_client_->set_hps_notify_result(initial_dbus_state_);

    AshTestBase::SetUp();

    controller_ = Shell::Get()->hps_notify_controller();

    // The controller has now been initialized, part of which entails sending a
    // method to the DBus service. Here we wait for the service to
    // asynchronously respond.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    chromeos::HpsDBusClient::Shutdown();
  }

 protected:
  const bool initial_dbus_state_;

  chromeos::FakeHpsDBusClient* dbus_client_ = nullptr;
  HpsNotifyController* controller_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// A test fixture where no snooper is initially detected.
class HpsNotifyControllerTestAbsent : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestAbsent()
      : HpsNotifyControllerTestBase(/*initial_dbus_state=*/false) {}
};

// Test that icon is hidden by default.
TEST_F(HpsNotifyControllerTestAbsent, Hidden) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(false);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(controller_->IsIconVisible());
}

// Test that messages from the daemon toggle the icon.
TEST_F(HpsNotifyControllerTestAbsent, HpsStateChange) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(controller_->IsIconVisible());

  controller_->OnHpsNotifyChanged(true);

  EXPECT_TRUE(controller_->IsIconVisible());

  controller_->OnHpsNotifyChanged(false);

  EXPECT_FALSE(controller_->IsIconVisible());
}

// A test fixture where a snooper is initially detected.
class HpsNotifyControllerTestPresent : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestPresent()
      : HpsNotifyControllerTestBase(/*initial_dbus_state=*/true) {}
};

// Test that initial daemon state is considered.
TEST_F(HpsNotifyControllerTestPresent, HpsState) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->IsIconVisible());
}

// Test that a user changing their preference toggles the icon.
TEST_F(HpsNotifyControllerTestPresent, PrefChanged) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(false);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(controller_->IsIconVisible());

  SetEnabledPref(true);

  EXPECT_TRUE(controller_->IsIconVisible());
}

// Test that eye icon isn't shown during the OOBE.
TEST_F(HpsNotifyControllerTestPresent, Oobe) {
  TestSessionControllerClient* session = GetSessionControllerClient();

  // Simulate end of OOBE when user is logged in.
  session->AddUserSession("testuser@gmail.com", user_manager::USER_TYPE_REGULAR,
                          /*provide_pref_service=*/true,
                          /*is_new_profile=*/true);
  session->SwitchActiveUser(AccountId::FromUserEmail("testuser@gmail.com"));
  session->SetSessionState(session_manager::SessionState::OOBE);

  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(controller_->IsIconVisible());

  session->SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_TRUE(controller_->IsIconVisible());
}

// Test that the eye icon isn't shown at the login page.
TEST_F(HpsNotifyControllerTestPresent, Login) {
  // Note: login deferred.

  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(controller_->IsIconVisible());

  SimulateUserLogin("testuser@gmail.com");

  // Don't show until new user has enabled their preference.
  EXPECT_FALSE(controller_->IsIconVisible());

  SetEnabledPref(true);
  EXPECT_TRUE(controller_->IsIconVisible());
}

}  // namespace ash
