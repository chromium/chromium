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
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"

namespace ash {

// Enables or disables the user pref for the feature. Because this could
// correctly or incorrectly trigger an asynchronous DBus call, waits for the run
// loop to empty.
void SetEnabledPref(bool enabled) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSnoopingProtectionEnabled, enabled);
  base::RunLoop().RunUntilIdle();
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
    dbus_client_->set_hps_notify_result(
        service_state_ ? hps::HpsResult::POSITIVE : hps::HpsResult::NEGATIVE);

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

  // Simulates a login. Because this could correctly or incorrectly trigger an
  // asynchronous DBus call, waits for the run loop to empty.
  void SimulateLogin() {
    SimulateUserLogin("testuser@gmail.com");
    base::RunLoop().RunUntilIdle();
  }
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
  SimulateLogin();
  SetEnabledPref(false);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->IsIconVisible());
}

// Test that messages from the daemon toggle the icon.
TEST_F(HpsNotifyControllerTestAbsent, HpsStateChange) {
  SimulateLogin();
  SetEnabledPref(true);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(controller_->IsIconVisible());

  controller_->OnHpsNotifyChanged(hps::HpsResult::POSITIVE);

  EXPECT_TRUE(controller_->IsIconVisible());

  controller_->OnHpsNotifyChanged(hps::HpsResult::NEGATIVE);

  EXPECT_FALSE(controller_->IsIconVisible());
}

// Test that daemon signals are only enabled when session and pref state means
// they will be used.
TEST_F(HpsNotifyControllerTestAbsent, ReconfigureOnPrefs) {
  // When the service becomes available for the first time, one disable is
  // performed in case the last session ended in a crash without de-configuring
  // the daemon.
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  SimulateLogin();
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  // Shouldn't configure or message the daemon until the user is ready to start
  // using the feature.
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  // Should de-configure the signal when it isn't being used.
  SetEnabledPref(false);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  // Should re-configure and re-poll when the signal becomes relevant again.
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 2);
}

// Test that daemon signals are correctly enabled/disabled when the daemon
// starts and stops.
TEST_F(HpsNotifyControllerTestAbsent, ReconfigureOnRestarts) {
  SimulateLogin();
  SetEnabledPref(true);

  // Should configure when we're both logged in and have our pref enabled. The
  // clean-up deconfigure always occurs.
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  // No deconfigure sent when the service shuts down, because it's unreachable.
  controller_->OnShutdown();
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  // Should reconfigure as soon as the service becomes available again.
  controller_->OnRestart();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 2);
}

// Test that the service is only re-configured when the user is _both_ logged-in
// and has enabled the preference.
TEST_F(HpsNotifyControllerTestAbsent, ReconfigureOnlyIfNecessary) {
  // Only the clean-up de-configure should have been sent.
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  // Not logged in, so should not configure the service.
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  // Only configure when both logged in and pref enabled.
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
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
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->IsIconVisible());
}

// Test that a user changing their preference toggles the icon.
TEST_F(HpsNotifyControllerTestPresent, PrefChanged) {
  SimulateLogin();
  SetEnabledPref(false);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->IsIconVisible());

  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

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

  // Shouldn't configure, as the session isn't active.
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->IsIconVisible());

  // Triggers an asynchronous DBus method call.
  session->SetSessionState(session_manager::SessionState::ACTIVE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->IsIconVisible());
}

// Test that the eye icon isn't shown at the login page.
TEST_F(HpsNotifyControllerTestPresent, Login) {
  // Note: login deferred.

  // Shouldn't configure, as the session isn't active.
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->IsIconVisible());

  SimulateLogin();

  // Don't show until new user has enabled their preference.
  EXPECT_FALSE(controller_->IsIconVisible());

  SetEnabledPref(true);
  EXPECT_TRUE(controller_->IsIconVisible());
}

// Test that the controller handles service restarts.
TEST_F(HpsNotifyControllerTestPresent, Restarts) {
  SimulateLogin();
  SetEnabledPref(true);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
  EXPECT_TRUE(controller_->IsIconVisible());

  // Icon is hidden when service goes down. Could erroneously trigger an
  // asynchronous DBus method call.
  dbus_client_->set_hps_service_is_available(false);
  controller_->OnShutdown();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(controller_->IsIconVisible());

  // Icon returns when service restarts. Controller now polls the DBus service
  // which responds asynchronously.
  dbus_client_->set_hps_service_is_available(true);
  controller_->OnRestart();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(dbus_client_->hps_notify_count(), 2);
  EXPECT_TRUE(controller_->IsIconVisible());
}

// Check that the controller state stays consistent even when the daemon starts
// and stops.
TEST_F(HpsNotifyControllerTestPresent, ClearHpsState) {
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(controller_->IsIconVisible(), true);

  // This should internally clear the cached daemon state.
  SetEnabledPref(false);
  EXPECT_EQ(controller_->IsIconVisible(), false);

  // Note: we don't exhaust the run loop here since we want to check the
  // controller state _before_ it is updated by asynchronous DBus calls.
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSnoopingProtectionEnabled, true);
  EXPECT_EQ(controller_->IsIconVisible(), false);
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

// Test that the controller waits for the DBus service to be available and
// doesn't communicate until it is.
TEST_F(HpsNotifyControllerTestUnavailable, WaitForService) {
  SimulateLogin();
  SetEnabledPref(true);

  // Shouldn't send any signals (even the clean-up deconfigure) to a service
  // that isn't available.
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->IsIconVisible());

  // Triggers an asynchronous DBus method call.
  dbus_client_->set_hps_service_is_available(true);
  controller_->OnRestart();
  base::RunLoop().RunUntilIdle();

  // Should now configure and send the initial poll.
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  // Controller now polls the DBus service which responds asynchronously.
  EXPECT_TRUE(controller_->IsIconVisible());
}

// Fixture with an invalid feature config.
class HpsNotifyControllerTestBadParams : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestBadParams()
      : HpsNotifyControllerTestBase(
            /*service_available=*/true,
            /*service_state=*/true,
            /*params=*/{{"filter_config_case", "0"}}) {}
};

// Test that the controller gracefully handles invalid feature parameters.
TEST_F(HpsNotifyControllerTestBadParams, BadParams) {
  SimulateLogin();
  SetEnabledPref(true);

  // Should send the clean-up disable even if we currently have a bad config.
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);
}

}  // namespace ash
