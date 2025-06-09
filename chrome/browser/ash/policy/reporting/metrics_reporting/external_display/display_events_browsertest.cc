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
#include "chromeos/ash/components/policy/device_policy/cached_device_policy_updater.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/constants/chromeos_features.h"
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
// when the ReportDeviceGraphicsStatus policy is set/unset.
static constexpr char kTestUserEmail[] = "test@example.com";
static constexpr char kTestAffiliationId[] = "test_affiliation_id";
static constexpr char kDMToken[] = "token";

class DisplayEventsBrowserTest : public policy::DevicePolicyCrosBrowserTest,
                                 public ::testing::WithParamInterface<bool> {
 protected:
  DisplayEventsBrowserTest()
      : test_account_id_(AccountId::FromUserEmailGaiaId(
            kTestUserEmail,
            signin::GetTestGaiaIdForEmail(kTestUserEmail))) {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          chromeos::features::kExternalDisplayEventTelemetry);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          chromeos::features::kExternalDisplayEventTelemetry);
    }
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

  void SetGraphicsStatusPolicy(bool enabled) {
    policy::CachedDevicePolicyUpdater updater;
    updater.payload().mutable_device_reporting()->set_report_graphics_status(
        enabled);
    updater.Commit();
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
      bool enable_graphics_status_policy,
      bool is_affiliated,
      std::optional<MetricEventType> expected_metric_event_type) {
    chromeos::MissiveClientTestObserver missive_observer(
        Destination::EVENT_METRIC);

    SetGraphicsStatusPolicy(enable_graphics_status_policy);

    if (is_affiliated) {
      LoginAffiliatedUser();
    } else {
      LoginUnaffiliatedUser();
    }

    EmitExternalDisplayEventForTesting(display_event_state);

    content::RunAllTasksUntilIdle();

    if (expected_metric_event_type.has_value()) {
      Record record = std::get<1>(missive_observer.GetNextEnqueuedRecord());
      ASSERT_TRUE(record.has_source_info());
      EXPECT_EQ(record.source_info().source(), SourceInfo::ASH);

      MetricData record_data;
      ASSERT_TRUE(record_data.ParseFromString(record.data()));

      ASSERT_TRUE(record_data.has_telemetry_data());
      EXPECT_TRUE(record_data.telemetry_data().has_displays_telemetry());
      EXPECT_EQ(record_data.event_data().type(),
                expected_metric_event_type.value());
      EXPECT_EQ(record.destination(), Destination::EVENT_METRIC);
      ASSERT_TRUE(record.has_dm_token());
      EXPECT_EQ(record.dm_token(), kDMToken);
    }

    EXPECT_FALSE(missive_observer.HasNewEnqueuedRecord());
  }

  bool IsExternalDisplayEventTelemetryEnabled() { return GetParam(); }

 private:
  const AccountId test_account_id_;
  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_, test_account_id_};
  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};
  ash::LoginManagerMixin login_manager_mixin_{
      &mixin_host_, ash::LoginManagerMixin::UserList(), &fake_gaia_mixin_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(
    DisplayEventsBrowserTest,
    DisplayConnectedEventCollectedWhenPolicyEnabledWithAffiliatedUser) {
  RunExternalDisplayEventCommon(
      ExternalDisplayEventInfo::State::kAdd,
      /*enable_graphics_status_policy=*/true,
      /*is_affiliated=*/true,
      IsExternalDisplayEventTelemetryEnabled()
          ? std::make_optional(MetricEventType::EXTERNAL_DISPLAY_CONNECTED)
          : std::nullopt);
}

IN_PROC_BROWSER_TEST_P(
    DisplayEventsBrowserTest,
    DisplayDisconnectedEventCollectedWhenPolicyEnabledWithAffiliatedUser) {
  RunExternalDisplayEventCommon(
      ExternalDisplayEventInfo::State::kRemove,
      /*enable_graphics_status_policy=*/true,
      /*is_affiliated=*/true,
      IsExternalDisplayEventTelemetryEnabled()
          ? std::make_optional(MetricEventType::EXTERNAL_DISPLAY_DISCONNECTED)
          : std::nullopt);
}

IN_PROC_BROWSER_TEST_P(DisplayEventsBrowserTest,
                       NoDisplayEventsWhenPolicyEnabledWithUnaffiliatedUser) {
  RunExternalDisplayEventCommon(ExternalDisplayEventInfo::State::kAdd,
                                /*enable_graphics_status_policy=*/true,
                                /*is_affiliated=*/false,
                                /*expected_metric_event_type=*/std::nullopt);
}

IN_PROC_BROWSER_TEST_P(DisplayEventsBrowserTest,
                       NoDisplayEventsWhenPolicyDisabledWithAffiliatedUser) {
  RunExternalDisplayEventCommon(ExternalDisplayEventInfo::State::kAdd,
                                /*enable_graphics_status_policy=*/false,
                                /*is_affiliated=*/true,
                                /*expected_metric_event_type=*/std::nullopt);
}

IN_PROC_BROWSER_TEST_P(DisplayEventsBrowserTest,
                       NoDisplayEventsWhenPolicyDisabledWithUnaffiliatedUser) {
  RunExternalDisplayEventCommon(ExternalDisplayEventInfo::State::kRemove,
                                /*enable_graphics_status_policy=*/false,
                                /*is_affiliated=*/false,
                                /*expected_metric_event_type=*/std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(
    DisplayEventsBrowserTest,
    DisplayEventsBrowserTest,
    ::testing::Values(/*enable_external_display_event_telemetry=*/true,
                      /*enable_external_display_event_telemetry=*/false));

}  // namespace
}  // namespace reporting
