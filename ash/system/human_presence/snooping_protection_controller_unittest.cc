// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/snooping_protection_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/human_presence/human_presence_metrics.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "chromeos/ash/components/dbus/human_presence/fake_human_presence_dbus_client.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"

namespace ash {
namespace {

namespace metrics = ash::snooping_protection_metrics;

// The minimum positive window length will be in the range of a few seconds.
// Here we define two windows that will surely be shorter and longer resp. than
// the positive window length.
constexpr base::TimeDelta kShortTime = base::Milliseconds(30);
constexpr base::TimeDelta kLongTime = base::Seconds(30);

// A fixture that provides access to a fake daemon and an instance of the
// controller hooked up to the test environment.
class SnoopingProtectionControllerTestBase : public NoSessionAshTestBase {
 public:
  // Arguments control the state of the feature and service on controller
  // construction. We can't set this value in individual tests since it must be
  // done before AshTestBase::SetUp() executes.
  SnoopingProtectionControllerTestBase(
      bool service_available,
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

  SnoopingProtectionControllerTestBase(
      const SnoopingProtectionControllerTestBase&) = delete;
  SnoopingProtectionControllerTestBase& operator=(
      const SnoopingProtectionControllerTestBase&) = delete;
  ~SnoopingProtectionControllerTestBase() override = default;

  void SetUp() override {
    HumanPresenceDBusClient::InitializeFake();
    dbus_client_ = FakeHumanPresenceDBusClient::Get();
    dbus_client_->set_hps_service_is_available(service_available_);
    hps::HpsResultProto state;
    state.set_value(service_state_ ? hps::HpsResult::POSITIVE
                                   : hps::HpsResult::NEGATIVE);
    dbus_client_->set_hps_notify_result(state);

    AshTestBase::SetUp();

    controller_ = Shell::Get()->snooping_protection_controller();

    // The controller has now been initialized, part of which entails sending a
    // method to the DBus service. Here we wait for the service to
    // asynchronously respond.
    task_environment()->FastForwardBy(kShortTime);
  }

  void TearDown() override {
    AshTestBase::TearDown();
    HumanPresenceDBusClient::Shutdown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;

  const bool service_available_;
  const bool service_state_;
  const std::map<std::string, std::string> params_;

  raw_ptr<FakeHumanPresenceDBusClient, DanglingUntriaged> dbus_client_ =
      nullptr;
  raw_ptr<SnoopingProtectionController, DanglingUntriaged> controller_ =
      nullptr;

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
class SnoopingProtectionControllerTestAbsent
    : public SnoopingProtectionControllerTestBase {
 public:
  SnoopingProtectionControllerTestAbsent()
      : SnoopingProtectionControllerTestBase(
            /*service_available=*/true,
            /*service_state=*/false,
            /*params=*/{{"SnoopingProtection_filter_config_case", "1"}}) {}
};

// Test that icon is hidden by default.
TEST_F(SnoopingProtectionControllerTestAbsent, Hidden) {
  SimulateLogin();
  SetEnabledPref(false);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->SnooperPresent());
}

// Test that messages from the daemon toggle the icon.
TEST_F(SnoopingProtectionControllerTestAbsent, PresenceChange) {
  SimulateLogin();
  SetEnabledPref(true);

  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_FALSE(controller_->SnooperPresent());

  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  task_environment()->FastForwardBy(kLongTime);

  EXPECT_TRUE(controller_->SnooperPresent());

  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);
  task_environment()->FastForwardBy(kLongTime);

  EXPECT_FALSE(controller_->SnooperPresent());
}

// Test that daemon signals are only enabled when session and pref state means
// they will be used.
//
// TODO(crbug.com/40254348): Flaky test.
TEST_F(SnoopingProtectionControllerTestAbsent, DISABLED_ReconfigureOnPrefs) {
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
//
// TODO(crbug.com/40254348): Flaky test.
TEST_F(SnoopingProtectionControllerTestAbsent, DISABLED_ReconfigureOnRestarts) {
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
TEST_F(SnoopingProtectionControllerTestAbsent, ReconfigureOnlyIfNecessary) {
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
class SnoopingProtectionControllerTestPresent
    : public SnoopingProtectionControllerTestBase {
 public:
  SnoopingProtectionControllerTestPresent()
      : SnoopingProtectionControllerTestBase(
            /*service_available=*/true,
            /*service_state=*/true,
            /*params=*/{{"SnoopingProtection_filter_config_case", "1"}}) {}
};

// Test that initial daemon state is considered.
TEST_F(SnoopingProtectionControllerTestPresent, PresenceState) {
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->SnooperPresent());
}

// Test that a user changing their preference toggles the icon.
TEST_F(SnoopingProtectionControllerTestPresent, PrefChanged) {
  SimulateLogin();
  SetEnabledPref(false);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);

  EXPECT_FALSE(controller_->SnooperPresent());

  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->SnooperPresent());
}

// Test that eye icon isn't shown during the OOBE.
TEST_F(SnoopingProtectionControllerTestPresent, Oobe) {
  TestSessionControllerClient* session = GetSessionControllerClient();

  // Simulate end of OOBE when user is logged in.
  session->AddUserSession("testuser@gmail.com",
                          user_manager::UserType::kRegular,
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
TEST_F(SnoopingProtectionControllerTestPresent, Login) {
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
TEST_F(SnoopingProtectionControllerTestPresent, Restarts) {
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
TEST_F(SnoopingProtectionControllerTestPresent, ClearPresenceState) {
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
TEST_F(SnoopingProtectionControllerTestPresent, Orientation) {
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
  EXPECT_TRUE(controller_->SnooperPresent());

  // When the orientation becomes unsuitable, we should disable the daemon.
  controller_->OnOrientationChanged(/*suitable_for_human_presence=*/false);
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);
  EXPECT_FALSE(controller_->SnooperPresent());

  // When the orientation becomes suitable again, we should re-enable the
  // daemon.
  controller_->OnOrientationChanged(/*suitable_for_human_presence=*/true);
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 2);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 2);
  EXPECT_TRUE(controller_->SnooperPresent());
}

// Test that the minimum positive window is respected.
TEST_F(SnoopingProtectionControllerTestPresent, PositiveWindow) {
  SimulateLogin();
  SetEnabledPref(true);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 1);

  EXPECT_TRUE(controller_->SnooperPresent());
  hps::HpsResultProto state;
  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);

  // The snooping status shouldn't immediately change, since we have a minimum
  // length that it should remain positive.
  task_environment()->FastForwardBy(kShortTime);
  EXPECT_TRUE(controller_->SnooperPresent());

  // After the window, it should become false.
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_FALSE(controller_->SnooperPresent());

  // Snooping status should always immediately become true and stay true.
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  EXPECT_TRUE(controller_->SnooperPresent());
  task_environment()->FastForwardBy(kLongTime);
  EXPECT_TRUE(controller_->SnooperPresent());

  // Snooping status should immediately become false if there is a service
  // reconfiguration (v.s. state change).
  controller_->OnShutdown();
  task_environment()->FastForwardBy(kShortTime);
  EXPECT_FALSE(controller_->SnooperPresent());
}

// Fixture with the DBus service initially unavailable (using a minimal set of
// valid params).
class SnoopingProtectionControllerTestUnavailable
    : public SnoopingProtectionControllerTestBase {
 public:
  SnoopingProtectionControllerTestUnavailable()
      : SnoopingProtectionControllerTestBase(
            /*service_available=*/false,
            /*service_state=*/true,
            /*params=*/{{"SnoopingProtection_filter_config_case", "1"}}) {}
};

// Test that the controller waits for the DBus service to be available and
// doesn't communicate until it is.
TEST_F(SnoopingProtectionControllerTestUnavailable, WaitForService) {
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
class SnoopingProtectionControllerTestBadParams
    : public SnoopingProtectionControllerTestBase {
 public:
  SnoopingProtectionControllerTestBadParams()
      : SnoopingProtectionControllerTestBase(
            /*service_available=*/true,
            /*service_state=*/true,
            /*params=*/{{"SnoopingProtection_filter_config_case", "0"}}) {}
};

// Test that the controller gracefully handles invalid feature parameters.
TEST_F(SnoopingProtectionControllerTestBadParams, BadParams) {
  SimulateLogin();
  SetEnabledPref(true);

  // Should send the clean-up disable even if we currently have a bad config.
  EXPECT_EQ(dbus_client_->enable_hps_notify_count(), 0);
  EXPECT_EQ(dbus_client_->disable_hps_notify_count(), 1);
  EXPECT_EQ(dbus_client_->hps_notify_count(), 0);
}

// Use the same texture as TestAbsent for UMA metrics.
class SnoopingProtectionControllerTestMetrics
    : public SnoopingProtectionControllerTestAbsent {};

TEST_F(SnoopingProtectionControllerTestMetrics, EnableDisablePref) {
  base::HistogramTester tester;

  SimulateLogin();
  tester.ExpectTotalCount(metrics::kEnabledHistogramName, 0);

  SetEnabledPref(true);
  tester.ExpectBucketCount(metrics::kEnabledHistogramName, 1, 1);
  tester.ExpectTotalCount(metrics::kEnabledHistogramName, 1);

  SetEnabledPref(false);
  tester.ExpectBucketCount(metrics::kEnabledHistogramName, 0, 1);
  tester.ExpectTotalCount(metrics::kEnabledHistogramName, 2);
}

TEST_F(SnoopingProtectionControllerTestMetrics, Duration) {
  base::HistogramTester tester;

  SimulateLogin();
  SetEnabledPref(true);
  hps::HpsResultProto state;

  // The first HpsNotifyChanged will not log anything.
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);

  task_environment()->FastForwardBy(kLongTime);

  // Send UNKNOWN will log a positive duration event.
  state.set_value(hps::HpsResult::UNKNOWN);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTimeBucketCount(metrics::kPositiveDurationHistogramName,
                               kLongTime, 1);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);

  task_environment()->FastForwardBy(kLongTime);

  // Send NEGATIVE a second time will not log anything.
  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);

  // Send POSITIVE will log a negative duration event.
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTimeBucketCount(metrics::kNegativeDurationHistogramName,
                               kLongTime, 1);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 1);
}

TEST_F(SnoopingProtectionControllerTestMetrics, ShutDownTest) {
  base::HistogramTester tester;

  SimulateLogin();
  SetEnabledPref(true);
  hps::HpsResultProto state;

  // The first HpsNotifyChanged will not log anything.
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);

  task_environment()->FastForwardBy(kLongTime);

  dbus_client_->Shutdown();
  tester.ExpectTimeBucketCount(metrics::kPositiveDurationHistogramName,
                               kLongTime, 1);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);
  dbus_client_->Restart();

  // Send NEGATIVE will not log anything because of the shutdown.
  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);

  task_environment()->FastForwardBy(kLongTime);

  // Send POSITIVE will log a negative duration event.
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTimeBucketCount(metrics::kNegativeDurationHistogramName,
                               kLongTime, 1);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 1);
}

TEST_F(SnoopingProtectionControllerTestMetrics, FlakeyDetection) {
  base::HistogramTester tester;

  SimulateLogin();
  SetEnabledPref(true);
  hps::HpsResultProto state;

  // The first HpsNotifyChanged will not log anything.
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kFlakeyHistogramName, 0);
  EXPECT_TRUE(controller_->SnooperPresent());

  task_environment()->FastForwardBy(kShortTime);
  // Send NEGATIVE after a short period of time will log a flakey detection.
  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kFlakeyHistogramName, 1);
  EXPECT_TRUE(controller_->SnooperPresent());

  task_environment()->FastForwardBy(kShortTime);
  // Send NEGATIVE again after a short period of time will log another flakey
  // detection.
  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kFlakeyHistogramName, 2);
  EXPECT_TRUE(controller_->SnooperPresent());

  task_environment()->FastForwardBy(kLongTime);
  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 0);
  tester.ExpectTotalCount(metrics::kFlakeyHistogramName, 2);
  EXPECT_FALSE(controller_->SnooperPresent());

  // Send POSITIVE after a short period of time will NOT log a flakey detection
  // for now.
  task_environment()->FastForwardBy(kShortTime);
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kPositiveDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kNegativeDurationHistogramName, 1);
  tester.ExpectTotalCount(metrics::kFlakeyHistogramName, 2);
  EXPECT_TRUE(controller_->SnooperPresent());
}

TEST_F(SnoopingProtectionControllerTestMetrics,
       FlakeyDetectionWithOtherSignals) {
  base::HistogramTester tester;

  SimulateLogin();
  SetEnabledPref(true);
  hps::HpsResultProto state;

  // The first HpsNotifyChanged will not log anything.
  state.set_value(hps::HpsResult::POSITIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kFlakeyHistogramName, 0);

  // Changing Orientation will disable HpsNotify and make the Present state to
  // be false.
  task_environment()->FastForwardBy(kShortTime);
  controller_->OnOrientationChanged(/*suitable_for_human_presence=*/false);
  controller_->OnOrientationChanged(/*suitable_for_human_presence=*/true);
  tester.ExpectTotalCount(metrics::kFlakeyHistogramName, 0);
  EXPECT_FALSE(controller_->SnooperPresent());

  // Send NEGATIVE after a short period of time will log a flakey detection
  // under this specific situation because the OrientationChange already put the
  // controller_->SnooperPresent state into false.
  state.set_value(hps::HpsResult::NEGATIVE);
  controller_->OnHpsNotifyChanged(state);
  tester.ExpectTotalCount(metrics::kFlakeyHistogramName, 0);
}

}  // namespace
}  // namespace ash
