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
  // Arguments control the state of the feature and service on controller
  // construction. We can't set this value in individual tests since it must be
  // done before AshTestBase::SetUp() executes.
  HpsNotifyControllerTestBase(bool service_available,
                              bool service_state,
                              const std::map<std::string, std::string>& params)
      : service_available_(service_available),
        service_state_(service_state),
        params_(params) {}
  HpsNotifyControllerTestBase(const HpsNotifyControllerTestBase&) = delete;
  HpsNotifyControllerTestBase& operator=(const HpsNotifyControllerTestBase&) =
      delete;
  ~HpsNotifyControllerTestBase() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ash::features::kSnoopingProtection, params_);

    chromeos::HpsDBusClient::InitializeFake();
    dbus_client_ = chromeos::FakeHpsDBusClient::Get();
    dbus_client_->set_hps_service_is_available(service_available_);
    dbus_client_->set_hps_notify_result(service_state_);

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
  const bool service_available_;
  const bool service_state_;
  const std::map<std::string, std::string> params_;

  chromeos::FakeHpsDBusClient* dbus_client_ = nullptr;
  HpsNotifyController* controller_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// A test fixture where no snooper is initially detected (using a minimal set of
// valid params).
class HpsNotifyControllerTestAbsent : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestAbsent()
      : HpsNotifyControllerTestBase(
            /*service_available=*/true,
            /*service_state=*/false,
            /*params=*/{{"filter_config_case", "1"}}) {}
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

// A test fixture where a snooper is initially detected (using a minimal set of
// valid params).
class HpsNotifyControllerTestPresent : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestPresent()
      : HpsNotifyControllerTestBase(
            /*service_available=*/true,
            /*service_state=*/true,
            /*params=*/{{"filter_config_case", "1"}}) {}
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

// Test that the controller handles service restarts.
TEST_F(HpsNotifyControllerTestPresent, Restarts) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(true);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
  EXPECT_TRUE(controller_->IsIconVisible());

  // Icon is hidden when service goes down.
  dbus_client_->set_hps_service_is_available(false);
  controller_->OnShutdown();
  EXPECT_FALSE(controller_->IsIconVisible());

  // Icon returns when service restarts.
  dbus_client_->set_hps_service_is_available(true);
  controller_->OnRestart();

  // Controller now polls the DBus service which responds asynchronously.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 2);
  EXPECT_TRUE(controller_->IsIconVisible());
}

// Fixture with the DBus service initially unavailable (using a minimal set of
// valid params).
class HpsNotifyControllerTestUnavailable : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestUnavailable()
      : HpsNotifyControllerTestBase(
            /*service_available=*/false,
            /*service_state=*/true,
            /*params=*/{{"filter_config_case", "1"}}) {}
};

// Test that the controller waits for the DBus service to be available.
TEST_F(HpsNotifyControllerTestUnavailable, WaitForService) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(true);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);
  EXPECT_FALSE(controller_->IsIconVisible());

  dbus_client_->set_hps_service_is_available(true);
  controller_->OnRestart();

  // Controller now polls the DBus service which responds asynchronously.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
  EXPECT_TRUE(controller_->IsIconVisible());
}

// Fixture with an invalid feature config.
class HpsNotifyControllerTestBadParams : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestBadParams()
      : HpsNotifyControllerTestBase(
            /*service_available=*/false,
            /*service_state=*/true,
            /*params=*/{}) {}
};

// Test that the controller gracefully handles invalid feature parameters.
TEST_F(HpsNotifyControllerTestBadParams, BadParams) {
  SimulateUserLogin("testuser@gmail.com");
  SetEnabledPref(true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);
  EXPECT_FALSE(controller_->IsIconVisible());
}

}  // namespace ash
