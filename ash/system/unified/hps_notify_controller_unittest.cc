// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/dbus/hps/fake_hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"

namespace ash {
namespace {

// The minimum positive window length will be in the range of a few seconds.
// Here we define two windows that will surely be shorter and longer resp. than
// the positive window length.
constexpr base::TimeDelta kShortTime = base::Milliseconds(30);
constexpr base::TimeDelta kLongTime = base::Seconds(30);

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
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        service_available_(service_available),
        service_state_(service_state),
        params_(params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ash::features::kSnoopingProtection, params_);
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kHasHps);
  }

  HpsNotifyControllerTestBase(const HpsNotifyControllerTestBase&) = delete;
  HpsNotifyControllerTestBase& operator=(const HpsNotifyControllerTestBase&) =
      delete;
  ~HpsNotifyControllerTestBase() override = default;

  void SetUp() override {
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
    task_environment()->FastForwardBy(kShortTime);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    chromeos::HpsDBusClient::Shutdown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;

  const bool service_available_;
  const bool service_state_;
  const std::map<std::string, std::string> params_;

  chromeos::FakeHpsDBusClient* dbus_client_ = nullptr;
  HpsNotifyController* controller_ = nullptr;

  // Simulates a login. This will trigger a DBus call if and only if logging in
  // was the final precondition required for the feature. Hence we wait for any
  // asynchronous logic to complete, revealing whether a DBus call was correctly
  // or incorrectly made.
  void SimulateLogin() {
    SimulateUserLogin("testuser@gmail.com");
    task_environment()->FastForwardBy(kShortTime);
  }

  // Enables or disables the user pref for the feature. This will trigger a DBus
  // call if and only if logging in was the final precondition required for the
  // feature. Hence we wait for any asynchronous logic to complete, revealing
  // whether a DBus call was correctly or incorrectly made.
  void SetEnabledPref(bool enabled) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        prefs::kSnoopingProtectionEnabled, enabled);
    task_environment()->FastForwardBy(kShortTime);
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
            /*params=*/{{"SnoopingProtection_filter_config_case", "1"}}) {}
};

// Test that icon is hidden by default.
TEST_F(HpsNotifyControllerTestAbsent, Hidden) {
  SimulateLogin();
  SetEnabledPref(false);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->SnooperPresent());
}

// Test that messages from the daemon toggle the icon.
TEST_F(HpsNotifyControllerTestAbsent, HpsStateChange) {
  SimulateLogin();
  SetEnabledPref(true);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(controller_->SnooperPresent());

  controller_->OnHpsNotifyChanged(hps::HpsResult::POSITIVE);
  task_environment()->FastForwardBy(kLongTime);

  EXPECT_TRUE(controller_->SnooperPresent());

  controller_->OnHpsNotifyChanged(hps::HpsResult::NEGATIVE);
  task_environment()->FastForwardBy(kLongTime);

  EXPECT_FALSE(controller_->SnooperPresent());
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
  task_environment()->FastForwardBy(kLongTime);
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
            /*params=*/{{"SnoopingProtection_filter_config_case", "1"}}) {}
};

// Test that initial daemon state is considered.
TEST_F(HpsNotifyControllerTestPresent, HpsState) {
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->SnooperPresent());
}

// Test that a user changing their preference toggles the icon.
TEST_F(HpsNotifyControllerTestPresent, PrefChanged) {
  SimulateLogin();
  SetEnabledPref(false);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->SnooperPresent());

  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->SnooperPresent());
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

  EXPECT_FALSE(controller_->SnooperPresent());

  // Triggers an asynchronous DBus method call.
  session->SetSessionState(session_manager::SessionState::ACTIVE);
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->SnooperPresent());
}

// Test that the eye icon isn't shown at the login page.
TEST_F(HpsNotifyControllerTestPresent, Login) {
  // Note: login deferred.

  // Shouldn't configure, as the session isn't active.
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->SnooperPresent());

  SimulateLogin();

  // Don't show until new user has enabled their preference.
  EXPECT_FALSE(controller_->SnooperPresent());

  SetEnabledPref(true);
  EXPECT_TRUE(controller_->SnooperPresent());
}

// Test that the controller handles service restarts.
TEST_F(HpsNotifyControllerTestPresent, Restarts) {
  SimulateLogin();
  SetEnabledPref(true);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
  EXPECT_TRUE(controller_->SnooperPresent());

  // Icon is hidden when service goes down. Could erroneously trigger an
  // asynchronous DBus method call.
  dbus_client_->set_hps_service_is_available(false);
  controller_->OnShutdown();
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_FALSE(controller_->SnooperPresent());

  // Icon returns when service restarts. Controller now polls the DBus service
  // which responds asynchronously.
  dbus_client_->set_hps_service_is_available(true);
  controller_->OnRestart();
  task_environment()->FastForwardBy(kLongTime);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 2);
  EXPECT_TRUE(controller_->SnooperPresent());
}

// Check that the controller state stays consistent even when the daemon starts
// and stops.
TEST_F(HpsNotifyControllerTestPresent, ClearHpsState) {
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(controller_->SnooperPresent(), true);

  // This should internally clear the cached daemon state.
  SetEnabledPref(false);
  EXPECT_EQ(controller_->SnooperPresent(), false);

  // Note: we don't exhaust the run loop here since we want to check the
  // controller state _before_ it is updated by asynchronous DBus calls.
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kSnoopingProtectionEnabled, true);
  EXPECT_EQ(controller_->SnooperPresent(), false);
}

// Test that detection is started and stopped based on whether the device's
// physical orientation is suitable for sensing.
TEST_F(HpsNotifyControllerTestPresent, Orientation) {
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
  EXPECT_TRUE(controller_->SnooperPresent());

  // When the orientation becomes unsuitable, we should disable the daemon.
  controller_->OnOrientationChanged(/*suitable_for_hps=*/false);
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
  EXPECT_FALSE(controller_->SnooperPresent());

  // When the orientation becomes suitable again, we should re-enable the
  // daemon.
  controller_->OnOrientationChanged(/*suitable_for_hps=*/true);
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 2);
  EXPECT_TRUE(controller_->SnooperPresent());
}

// Test that the minimum positive window is respected.
TEST_F(HpsNotifyControllerTestPresent, PositiveWindow) {
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->SnooperPresent());
  controller_->OnHpsNotifyChanged(hps::HpsResult::NEGATIVE);

  // The snooping status shouldn't immediately change, since we have a minimum
  // length that it should remain positive.
  task_environment()->FastForwardBy(kShortTime);
  EXPECT_TRUE(controller_->SnooperPresent());

  // After the window, it should become false.
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_FALSE(controller_->SnooperPresent());

  // Snooping status should always immediately become true and stay true.
  controller_->OnHpsNotifyChanged(hps::HpsResult::POSITIVE);
  EXPECT_TRUE(controller_->SnooperPresent());
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_TRUE(controller_->SnooperPresent());

  // Snooping status should immediately become false if there is an HPS
  // reconfiguration (v.s. state change).
  controller_->OnShutdown();
  task_environment()->FastForwardBy(kShortTime);
  EXPECT_FALSE(controller_->SnooperPresent());
}

// Fixture with the DBus service initially unavailable (using a minimal set of
// valid params).
class HpsNotifyControllerTestUnavailable : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestUnavailable()
      : HpsNotifyControllerTestBase(
            /*service_available=*/false,
            /*service_state=*/true,
            /*params=*/{{"SnoopingProtection_filter_config_case", "1"}}) {}
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

  EXPECT_FALSE(controller_->SnooperPresent());

  // Triggers an asynchronous DBus method call.
  dbus_client_->set_hps_service_is_available(true);
  controller_->OnRestart();
  task_environment()->FastForwardBy(kLongTime);

  // Should now configure and send the initial poll.
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  // Controller now polls the DBus service which responds asynchronously.
  EXPECT_TRUE(controller_->SnooperPresent());
}

// Fixture with an invalid feature config.
class HpsNotifyControllerTestBadParams : public HpsNotifyControllerTestBase {
 public:
  HpsNotifyControllerTestBadParams()
      : HpsNotifyControllerTestBase(
            /*service_available=*/true,
            /*service_state=*/true,
            /*params=*/{{"SnoopingProtection_filter_config_case", "0"}}) {}
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

}  // namespace
}  // namespace ash
