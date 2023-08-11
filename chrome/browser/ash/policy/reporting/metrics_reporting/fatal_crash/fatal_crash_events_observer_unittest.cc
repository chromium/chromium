// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using ::ash::cros_healthd::FakeCrosHealthd;
using ::ash::cros_healthd::mojom::CrashEventInfo;
using ::ash::cros_healthd::mojom::CrashEventInfoPtr;
using ::ash::cros_healthd::mojom::CrashUploadInfo;
using ::ash::cros_healthd::mojom::EventCategoryEnum;
using ::ash::cros_healthd::mojom::EventInfo;

// Tests fatal crash events observer. `NoSessionAshTestBase` is needed here
// because the observer uses `ash::Shell()` to obtain the user session type.
class FatalCrashEventsObserverTest
    : public ::ash::NoSessionAshTestBase,
      public ::testing::WithParamInterface</*uploaded=*/bool> {
 public:
  FatalCrashEventsObserverTest(const FatalCrashEventsObserverTest&) = delete;
  FatalCrashEventsObserverTest& operator=(const FatalCrashEventsObserverTest&) =
      delete;

 protected:
  static constexpr std::string_view kCrashReportId = "Crash Report ID";
  static constexpr std::string_view kUserEmail = "user@example.com";

  FatalCrashEventsObserverTest() = default;
  ~FatalCrashEventsObserverTest() override = default;

  void SetUp() override {
    ::ash::NoSessionAshTestBase::SetUp();
    FakeCrosHealthd::Initialize();
  }

  void TearDown() override {
    FakeCrosHealthd::Shutdown();
    ::ash::NoSessionAshTestBase::TearDown();
  }

  bool is_uploaded() const { return GetParam(); }

  // Let the fake cros_healthd emit the crash event and wait for the
  // `FatalCrashTelemetry` message to become available.
  FatalCrashTelemetry WaitForFatalCrashTelemetry(
      CrashEventInfoPtr crash_event_info) {
    test::TestEvent<MetricData> result_metric_data;
    FatalCrashEventsObserver fatal_crash_observer;
    fatal_crash_observer.SetOnEventObservedCallback(
        result_metric_data.repeating_cb());
    fatal_crash_observer.SetReportingEnabled(true);

    FakeCrosHealthd::Get()->EmitEventForCategory(
        EventCategoryEnum::kCrash,
        EventInfo::NewCrashEventInfo(std::move(crash_event_info)));

    auto metric_data = result_metric_data.result();
    EXPECT_TRUE(metric_data.has_telemetry_data());
    EXPECT_TRUE(metric_data.telemetry_data().has_fatal_crash_telemetry());
    return std::move(metric_data.telemetry_data().fatal_crash_telemetry());
  }

  // Create a new `CrashEventInfo` object that respects the `is_uploaded`
  // param.
  CrashEventInfoPtr NewCrashEventInfo() {
    auto crash_event_info = CrashEventInfo::New();
    if (is_uploaded()) {
      crash_event_info->upload_info = CrashUploadInfo::New();
      crash_event_info->upload_info->crash_report_id = kCrashReportId;
    }

    return crash_event_info;
  }

  // Similar to SimulateUserLogin, except the user is affiliated.
  void SimulateAffiliatedUserLogin(std::string_view user_email,
                                   user_manager::UserType user_type) {
    const auto account_id = AccountId::FromUserEmail(std::string(user_email));
    GetSessionControllerClient()->AddUserSession(
        account_id, account_id.GetUserEmail(), user_type,
        /*provide_pref_service=*/true, /*is_new_profile=*/false,
        /*given_name=*/std::string(), /*is_managed=*/true);
    GetSessionControllerClient()->SwitchActiveUser(account_id);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::ACTIVE);
  }

 private:
  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

TEST_P(FatalCrashEventsObserverTest, FieldTypePassedThrough) {
  auto crash_event_info = NewCrashEventInfo();
  crash_event_info->crash_type = CrashEventInfo::CrashType::kKernel;

  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  ASSERT_TRUE(fatal_crash_telemetry.has_type());
  EXPECT_EQ(fatal_crash_telemetry.type(),
            FatalCrashTelemetry::CRASH_TYPE_KERNEL);
}

TEST_P(FatalCrashEventsObserverTest, FieldLocalIdPassedThrough) {
  static constexpr std::string_view kLocalId = "local ID a";

  auto crash_event_info = NewCrashEventInfo();
  crash_event_info->local_id = kLocalId;

  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
  EXPECT_EQ(fatal_crash_telemetry.local_id(), kLocalId);
}

TEST_P(FatalCrashEventsObserverTest, FieldTimestampPassedThrough) {
  static constexpr base::Time kCaptureTime = base::Time::FromTimeT(2);

  auto crash_event_info = NewCrashEventInfo();
  crash_event_info->capture_time = kCaptureTime;

  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  ASSERT_TRUE(fatal_crash_telemetry.has_timestamp_us());
  EXPECT_EQ(fatal_crash_telemetry.timestamp_us(), kCaptureTime.ToJavaTime());
}

TEST_P(FatalCrashEventsObserverTest, FieldCrashReportIdPassedThrough) {
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(NewCrashEventInfo());
  if (is_uploaded()) {
    ASSERT_TRUE(fatal_crash_telemetry.has_crash_report_id());
    EXPECT_EQ(fatal_crash_telemetry.crash_report_id(), kCrashReportId);
  } else {
    // No report ID for unuploaded crashes.
    EXPECT_FALSE(fatal_crash_telemetry.has_crash_report_id());
  }
}

TEST_P(FatalCrashEventsObserverTest, FieldUserEmailFilledIfAffiliated) {
  SimulateAffiliatedUserLogin(kUserEmail, user_manager::USER_TYPE_REGULAR);
  auto crash_event_info = NewCrashEventInfo();
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));

  ASSERT_TRUE(fatal_crash_telemetry.has_affiliated_user());
  ASSERT_TRUE(fatal_crash_telemetry.affiliated_user().has_user_email());
  EXPECT_EQ(fatal_crash_telemetry.affiliated_user().user_email(), kUserEmail);
}

TEST_P(FatalCrashEventsObserverTest, FieldUserEmailAbsentIfUnaffiliated) {
  SimulateUserLogin(std::string(kUserEmail), user_manager::USER_TYPE_REGULAR);
  auto crash_event_info = NewCrashEventInfo();
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  EXPECT_FALSE(fatal_crash_telemetry.has_affiliated_user());
}

TEST_P(FatalCrashEventsObserverTest, FieldSessionTypeFilledIfAffiliated) {
  // Sample 2 session types. Otherwise it would be repeating `GetSessionType`
  // in fatal_crash_events_observer.cc.
  static constexpr std::tuple<user_manager::UserType,
                              FatalCrashTelemetry::SessionType>
      kSessionTypes[] = {{user_manager::USER_TYPE_CHILD,
                          FatalCrashTelemetry::SESSION_TYPE_CHILD},
                         {user_manager::USER_TYPE_GUEST,
                          FatalCrashTelemetry::SESSION_TYPE_GUEST}};

  for (size_t i = 0; i < std::size(kSessionTypes); ++i) {
    SimulateAffiliatedUserLogin(std::string(kUserEmail),
                                std::get<0>(kSessionTypes[i]));
    auto crash_event_info = NewCrashEventInfo();
    const auto fatal_crash_telemetry =
        WaitForFatalCrashTelemetry(std::move(crash_event_info));
    ASSERT_TRUE(fatal_crash_telemetry.has_session_type());
    EXPECT_EQ(fatal_crash_telemetry.session_type(),
              std::get<1>(kSessionTypes[i]));
    ClearLogin();
  }
}

TEST_P(FatalCrashEventsObserverTest,
       FieldSessionTypeUnspecifiedIfUnaffiliated) {
  SimulateUserLogin(std::string(kUserEmail), user_manager::USER_TYPE_REGULAR);
  auto crash_event_info = NewCrashEventInfo();
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  ASSERT_TRUE(fatal_crash_telemetry.has_session_type());
  EXPECT_EQ(fatal_crash_telemetry.session_type(),
            FatalCrashTelemetry::SESSION_TYPE_UNSPECIFIED);
}

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverTests,
    FatalCrashEventsObserverTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<FatalCrashEventsObserverTest::ParamType>&
           info) { return info.param ? "uploaded" : "unuploaded"; });
}  // namespace
}  // namespace reporting
