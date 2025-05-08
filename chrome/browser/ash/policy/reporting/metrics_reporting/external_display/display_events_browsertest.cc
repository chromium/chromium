// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/containers/contains.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/external_display/display_events_observer.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/account_id/account_id.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using ::ash::cros_healthd::mojom::EventCategoryEnum;
using ::ash::cros_healthd::mojom::EventInfo;
using ::ash::cros_healthd::mojom::ExternalDisplayEventInfo;
using ::ash::cros_healthd::mojom::ExternalDisplayInfo;

// Browser test that validates external display connected/disconnected events
// when the ReportDevicePeripherals policy is set/unset.
static constexpr char kTestUserEmail[] = "test@example.com";
static constexpr char kTestAffiliationId[] = "test_affiliation_id";
static constexpr char kDMToken[] = "token";

class DisplayEventsBrowserTest : public policy::DevicePolicyCrosBrowserTest {
 protected:
  DisplayEventsBrowserTest()
      : test_account_id_(AccountId::FromUserEmailGaiaId(
            kTestUserEmail,
            signin::GetTestGaiaIdForEmail(kTestUserEmail))) {
    // Add unaffiliated user for testing purposes.
    login_manager_mixin_.AppendRegularUsers(1);
    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDMToken));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    // Set up affiliation for the test user.
    auto device_policy_update = device_state_.RequestDevicePolicyUpdate();
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();

    device_policy_update->policy_data()->add_device_affiliation_ids(
        kTestAffiliationId);
    user_policy_update->policy_data()->add_user_affiliation_ids(
        kTestAffiliationId);
  }

  void EnableGraphicsStatusPolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceGraphicsStatus, true);
  }

  void DisableGraphicsStatusPolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportDeviceGraphicsStatus, false);
  }

  void LoginAffiliatedUser() {
    const ash::LoginManagerMixin::TestUserInfo user_info(test_account_id_);
    const auto& context =
        ash::LoginManagerMixin::CreateDefaultUserContext(user_info);
    login_manager_mixin_.LoginAsNewRegularUser(context);
    ash::test::WaitForPrimaryUserSessionStart();
  }

  void LoginUnaffiliatedUser() {
    login_manager_mixin_.LoginAsNewRegularUser();
    ash::test::WaitForPrimaryUserSessionStart();
  }

  void EmitExternalDisplayEventForTesting(
      ExternalDisplayEventInfo::State state) {
    ash::cros_healthd::FakeCrosHealthd::Get()->EmitEventForCategory(
        EventCategoryEnum::kExternalDisplay,
        EventInfo::NewExternalDisplayEventInfo(
            ExternalDisplayEventInfo::New(state, ExternalDisplayInfo::New())));
  }

  void RunExternalDisplayEventCommon(
      ExternalDisplayEventInfo::State display_event_state,
      MetricEventType expected_metric_event_type) {
    chromeos::MissiveClientTestObserver missive_observer(
        Destination::PERIPHERAL_EVENTS);

    EnableGraphicsStatusPolicy();

    LoginAffiliatedUser();

    EmitExternalDisplayEventForTesting(display_event_state);

    content::RunAllTasksUntilIdle();

    Record record = std::get<1>(missive_observer.GetNextEnqueuedRecord());
    ASSERT_TRUE(record.has_source_info());
    EXPECT_EQ(record.source_info().source(), SourceInfo::ASH);

    MetricData record_data;
    ASSERT_TRUE(record_data.ParseFromString(record.data()));

    EXPECT_TRUE(record_data.has_info_data());
    EXPECT_TRUE(record_data.info_data().has_display_info());
    EXPECT_EQ(record_data.event_data().type(), expected_metric_event_type);
    EXPECT_EQ(record.destination(), Destination::PERIPHERAL_EVENTS);
    ASSERT_TRUE(record.has_dm_token());
    EXPECT_EQ(record.dm_token(), kDMToken);

    EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
  }

 private:
  const AccountId test_account_id_;
  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_, test_account_id_};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};
  ash::LoginManagerMixin login_manager_mixin_{
      &mixin_host_, ash::LoginManagerMixin::UserList(), &fake_gaia_mixin_};
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(
    DisplayEventsBrowserTest,
    DisplayConnectedEventCollectedWhenPolicyEnabledWithAffiliatedUser) {
  RunExternalDisplayEventCommon(ExternalDisplayEventInfo::State::kAdd,
                                MetricEventType::EXTERNAL_DISPLAY_CONNECTED);
}

IN_PROC_BROWSER_TEST_F(
    DisplayEventsBrowserTest,
    DisplayDisconnectedEventCollectedWhenPolicyEnabledWithAffiliatedUser) {
  RunExternalDisplayEventCommon(ExternalDisplayEventInfo::State::kRemove,
                                MetricEventType::EXTERNAL_DISPLAY_DISCONNECTED);
}

IN_PROC_BROWSER_TEST_F(DisplayEventsBrowserTest,
                       NoDisplayEventsWhenPolicyEnabledWithUnaffiliatedUser) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  EnableGraphicsStatusPolicy();

  LoginUnaffiliatedUser();

  EmitExternalDisplayEventForTesting(ExternalDisplayEventInfo::State::kAdd);

  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(DisplayEventsBrowserTest,
                       NoDisplayEventsWhenPolicyDisabledWithAffiliatedUser) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  DisableGraphicsStatusPolicy();

  LoginAffiliatedUser();

  EmitExternalDisplayEventForTesting(ExternalDisplayEventInfo::State::kAdd);

  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

IN_PROC_BROWSER_TEST_F(DisplayEventsBrowserTest,
                       NoDisplayEventsWhenPolicyDisabledWithUnaffiliatedUser) {
  chromeos::MissiveClientTestObserver missive_observer(
      Destination::PERIPHERAL_EVENTS);

  DisableGraphicsStatusPolicy();

  LoginUnaffiliatedUser();

  EmitExternalDisplayEventForTesting(ExternalDisplayEventInfo::State::kRemove);

  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
}

}  // namespace
}  // namespace reporting
