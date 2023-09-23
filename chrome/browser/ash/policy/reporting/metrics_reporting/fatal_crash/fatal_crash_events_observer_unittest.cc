// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_test_util.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
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

// Base class for testing `FatalCrashEventsObserver`. `NoSessionAshTestBase` is
// needed here because the observer uses `ash::Shell()` to obtain the user
// session type.
class FatalCrashEventsObserverTestBase : public ::ash::NoSessionAshTestBase {
 public:
  FatalCrashEventsObserverTestBase(const FatalCrashEventsObserverTestBase&) =
      delete;
  FatalCrashEventsObserverTestBase& operator=(
      const FatalCrashEventsObserverTestBase&) = delete;

 protected:
  static constexpr std::string_view kCrashReportId = "Crash Report ID";
  static constexpr std::string_view kUserEmail = "user@example.com";

  FatalCrashEventsObserverTestBase() = default;
  ~FatalCrashEventsObserverTestBase() override = default;

  void SetUp() override {
    ::ash::NoSessionAshTestBase::SetUp();
    FakeCrosHealthd::Initialize();
  }

  void TearDown() override {
    FakeCrosHealthd::Shutdown();
    ::ash::NoSessionAshTestBase::TearDown();
  }

  // Create a `FatalCrashEventsObserver` object and enables reporting. It
  // optionally sets the OnEventObserved callback if test_event is provided.
  std::unique_ptr<FatalCrashEventsObserver>
  CreateAndEnableFatalCrashEventsObserver(
      base::test::TestFuture<MetricData>* test_event = nullptr) {
    auto observer =
        fatal_crash_test_environment_.CreateFatalCrashEventsObserver();
    observer->SetReportingEnabled(true);
    if (test_event) {
      observer->SetOnEventObservedCallback(test_event->GetRepeatingCallback());
    }
    return observer;
  }

  // Let the fake cros_healthd emit the crash event and wait for the
  // `FatalCrashTelemetry` message to become available.
  //
  // If fatal_crash_events_observer is null, then it creates the
  // `FatalCrashEventsObserver` object internally and enables reporting. If
  // test_event is null, then it creates the
  // `base::test::TestFuture<MetricData>` object internally and sets the
  // observer's OnEventObserved callback accordingly. If test_event is provided,
  // does not set the observer's OnEventObserved callback, which should be set
  // by the caller. This is useful when the caller needs to wait for fatal crash
  // telemetry for multiple times from the same observer, as the observer's
  // OnEventObserved callback cannot be set twice.
  FatalCrashTelemetry WaitForFatalCrashTelemetry(
      CrashEventInfoPtr crash_event_info,
      FatalCrashEventsObserver* fatal_crash_events_observer = nullptr,
      base::test::TestFuture<MetricData>* result_metric_data = nullptr) {
    std::unique_ptr<FatalCrashEventsObserver> internal_observer;
    if (fatal_crash_events_observer == nullptr) {
      internal_observer = CreateAndEnableFatalCrashEventsObserver();
      fatal_crash_events_observer = internal_observer.get();
    }

    std::unique_ptr<base::test::TestFuture<MetricData>> internal_test_event;
    if (result_metric_data == nullptr) {
      internal_test_event.reset(new base::test::TestFuture<MetricData>());
      result_metric_data = internal_test_event.get();
      fatal_crash_events_observer->SetOnEventObservedCallback(
          result_metric_data->GetRepeatingCallback());
    }

    FakeCrosHealthd::Get()->EmitEventForCategory(
        EventCategoryEnum::kCrash,
        EventInfo::NewCrashEventInfo(std::move(crash_event_info)));

    auto metric_data = result_metric_data->Take();
    EXPECT_TRUE(metric_data.has_telemetry_data());
    EXPECT_TRUE(metric_data.telemetry_data().has_fatal_crash_telemetry());
    return std::move(metric_data.telemetry_data().fatal_crash_telemetry());
  }

  // Create a new `CrashEventInfo` object that respects the `is_uploaded`
  // param.
  CrashEventInfoPtr NewCrashEventInfo(bool is_uploaded) {
    auto crash_event_info = CrashEventInfo::New();
    if (is_uploaded) {
      crash_event_info->upload_info = CrashUploadInfo::New();
      crash_event_info->upload_info->crash_report_id = kCrashReportId;
    }

    return crash_event_info;
  }

  // Simulate user login and allows specifying whether the user is affiliated.
  void SimulateUserLogin(std::string_view user_email,
                         user_manager::UserType user_type,
                         bool is_user_affiliated) {
    if (is_user_affiliated) {
      SimulateAffiliatedUserLogin(user_email, user_type);
    } else {
      // Calls the proxy of the parent's `SimulateUserLogin`.
      SimulateUserLogin(std::string(user_email), user_type);
    }
  }

  FatalCrashEventsObserver::TestEnvironment fatal_crash_test_environment_;

 private:
  // Similar to `AshTestBase::SimulateUserLogin`, except the user is
  // affiliated.
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

  // A proxy of parent's `AshTestBase::SimulateUserLogin`. This is to make it
  // private so that it won't be accidentally called, because every user login
  // simulation in the tests should specify whether the user is affiliated. Use
  // `SimulateUserLogin` defined in this class instead.
  void SimulateUserLogin(const std::string& user_email,
                         user_manager::UserType user_type) {
    NoSessionAshTestBase::SimulateUserLogin(user_email, user_type);
  }

  ::ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
};

// Tests `FatalCrashEventsObserver` with `uploaded` being parameterized.
class FatalCrashEventsObserverTest
    : public FatalCrashEventsObserverTestBase,
      public ::testing::WithParamInterface</*uploaded=*/bool> {
 public:
  FatalCrashEventsObserverTest(const FatalCrashEventsObserverTest&) = delete;
  FatalCrashEventsObserverTest& operator=(const FatalCrashEventsObserverTest&) =
      delete;

 protected:
  FatalCrashEventsObserverTest() = default;
  ~FatalCrashEventsObserverTest() override = default;

  bool is_uploaded() const { return GetParam(); }
};

TEST_P(FatalCrashEventsObserverTest, FieldTypePassedThrough) {
  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  crash_event_info->crash_type = CrashEventInfo::CrashType::kKernel;

  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  ASSERT_TRUE(fatal_crash_telemetry.has_type());
  EXPECT_EQ(fatal_crash_telemetry.type(),
            FatalCrashTelemetry::CRASH_TYPE_KERNEL);
}

TEST_P(FatalCrashEventsObserverTest, FieldLocalIdPassedThrough) {
  static constexpr std::string_view kLocalId = "local ID a";

  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  crash_event_info->local_id = kLocalId;

  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
  EXPECT_EQ(fatal_crash_telemetry.local_id(), kLocalId);
}

TEST_P(FatalCrashEventsObserverTest, FieldTimestampPassedThrough) {
  static constexpr base::Time kCaptureTime = base::Time::FromTimeT(2);

  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  crash_event_info->capture_time = kCaptureTime;

  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  ASSERT_TRUE(fatal_crash_telemetry.has_timestamp_us());
  EXPECT_EQ(fatal_crash_telemetry.timestamp_us(),
            FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTime));
}

TEST_P(FatalCrashEventsObserverTest, FieldCrashReportIdPassedThrough) {
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(NewCrashEventInfo(is_uploaded()));
  if (is_uploaded()) {
    ASSERT_TRUE(fatal_crash_telemetry.has_crash_report_id());
    EXPECT_EQ(fatal_crash_telemetry.crash_report_id(), kCrashReportId);
  } else {
    // No report ID for unuploaded crashes.
    EXPECT_FALSE(fatal_crash_telemetry.has_crash_report_id());
  }
}

TEST_P(FatalCrashEventsObserverTest, FieldUserEmailFilledIfAffiliated) {
  SimulateUserLogin(kUserEmail, user_manager::USER_TYPE_REGULAR,
                    /*is_user_affiliated=*/true);
  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));

  ASSERT_TRUE(fatal_crash_telemetry.has_affiliated_user());
  ASSERT_TRUE(fatal_crash_telemetry.affiliated_user().has_user_email());
  EXPECT_EQ(fatal_crash_telemetry.affiliated_user().user_email(), kUserEmail);
}

TEST_P(FatalCrashEventsObserverTest, FieldUserEmailAbsentIfUnaffiliated) {
  SimulateUserLogin(kUserEmail, user_manager::USER_TYPE_REGULAR,
                    /*is_user_affiliated=*/false);
  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  EXPECT_FALSE(fatal_crash_telemetry.has_affiliated_user());
}

TEST_P(FatalCrashEventsObserverTest, ObserveMultipleEvents) {
  base::test::TestFuture<MetricData> test_event;
  auto observer = CreateAndEnableFatalCrashEventsObserver(&test_event);

  for (int i = 0; i < 10; ++i) {
    const auto local_id = base::NumberToString(i);
    auto crash_event_info = NewCrashEventInfo(is_uploaded());
    crash_event_info->local_id = local_id;
    const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
        std::move(crash_event_info), observer.get(), &test_event);
    ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
    EXPECT_EQ(fatal_crash_telemetry.local_id(), local_id);
  }
}

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverTests,
    FatalCrashEventsObserverTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<FatalCrashEventsObserverTest::ParamType>&
           info) { return info.param ? "uploaded" : "unuploaded"; });

// Tests `FatalCrashEventsObserver` with both uploaded and user affiliation
// parameterized. Useful when testing behaviors that require a user session and
// that are homogeneous regarding user affiliation.
class FatalCrashEventsObserverWithUserAffiliationParamTest
    : public FatalCrashEventsObserverTestBase,
      public ::testing::WithParamInterface<
          std::tuple</*uploaded=*/bool,
                     /*user_affiliated=*/bool>> {
 public:
  FatalCrashEventsObserverWithUserAffiliationParamTest(
      const FatalCrashEventsObserverWithUserAffiliationParamTest&) = delete;
  FatalCrashEventsObserverWithUserAffiliationParamTest& operator=(
      const FatalCrashEventsObserverWithUserAffiliationParamTest&) = delete;

 protected:
  FatalCrashEventsObserverWithUserAffiliationParamTest() = default;
  ~FatalCrashEventsObserverWithUserAffiliationParamTest() override = default;

  bool is_uploaded() const { return std::get<0>(GetParam()); }
  bool is_user_affiliated() const { return std::get<1>(GetParam()); }
};

TEST_P(FatalCrashEventsObserverWithUserAffiliationParamTest,
       FieldSessionTypeFilled) {
  // Sample 2 session types. Otherwise it would be repeating `GetSessionType`
  // in fatal_crash_events_observer.cc.
  struct TypePairs {
    user_manager::UserType user_type;
    FatalCrashTelemetry::SessionType session_type;
  };
  static constexpr TypePairs kSessionTypes[] = {
      {.user_type = user_manager::USER_TYPE_CHILD,
       .session_type = FatalCrashTelemetry::SESSION_TYPE_CHILD},
      {.user_type = user_manager::USER_TYPE_GUEST,
       .session_type = FatalCrashTelemetry::SESSION_TYPE_GUEST}};

  for (size_t i = 0; i < std::size(kSessionTypes); ++i) {
    SimulateUserLogin(kUserEmail, kSessionTypes[i].user_type,
                      is_user_affiliated());
    auto crash_event_info = NewCrashEventInfo(is_uploaded());
    // crash with the same local ID would be ignored, assign a unique local ID
    // here to prevent the second session type from failure.
    crash_event_info->local_id = base::NumberToString(i);
    const auto fatal_crash_telemetry =
        WaitForFatalCrashTelemetry(std::move(crash_event_info));
    ASSERT_TRUE(fatal_crash_telemetry.has_session_type());
    EXPECT_EQ(fatal_crash_telemetry.session_type(),
              kSessionTypes[i].session_type);
    ClearLogin();
  }
}

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverWithUserAffiliationParamTests,
    FatalCrashEventsObserverWithUserAffiliationParamTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    [](const testing::TestParamInfo<
        FatalCrashEventsObserverWithUserAffiliationParamTest::ParamType>&
           info) {
      std::ostringstream ss;
      ss << (std::get<0>(info.param) ? "uploaded" : "unuploaded") << '_'
         << (std::get<1>(info.param) ? "user_affiliated" : "user_unaffiliated");
      return ss.str();
    });

// Tests `FatalCrashEventsObserver` with unuploaded crashes with a focus on
// saved reported local IDs.
class FatalCrashEventsObserverReportedLocalIdsTestBase
    : public FatalCrashEventsObserverTestBase {
 public:
  FatalCrashEventsObserverReportedLocalIdsTestBase(
      const FatalCrashEventsObserverReportedLocalIdsTestBase&) = delete;
  FatalCrashEventsObserverReportedLocalIdsTestBase& operator=(
      const FatalCrashEventsObserverReportedLocalIdsTestBase&) = delete;

 protected:
  // The maximum number of local IDs to save.
  static constexpr size_t kMaxNumOfLocalIds{
      FatalCrashEventsObserver::TestEnvironment::kMaxNumOfLocalIds};
  static constexpr std::string_view kLocalId = "local ID";
  static constexpr base::Time kCaptureTime = base::Time::FromTimeT(14);
  static constexpr std::string_view kLocalIdEarly = "local ID Early";
  static constexpr base::Time kCaptureTimeEarly = base::Time::FromTimeT(10);
  static constexpr std::string_view kLocalIdLate = "local ID Late";
  static constexpr base::Time kCaptureTimeLate = base::Time::FromTimeT(20);

  FatalCrashEventsObserverReportedLocalIdsTestBase() = default;
  ~FatalCrashEventsObserverReportedLocalIdsTestBase() override = default;

  // Gets the path to the save file.
  const base::FilePath& GetSaveFilePath() const {
    return fatal_crash_test_environment_.GetSaveFilePath();
  }

  // Generates an uninteresting fatal crash event to alter the observer's state
  // in preparation for the test.
  void CreateFatalCrashEvent(
      std::string_view local_id,
      base::Time capture_time,
      FatalCrashEventsObserver& fatal_crash_observer,
      base::test::TestFuture<MetricData>* test_event = nullptr) {
    auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
    crash_event_info->local_id = local_id;
    crash_event_info->capture_time = capture_time;

    const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
        std::move(crash_event_info), &fatal_crash_observer, test_event);
    ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
    ASSERT_EQ(fatal_crash_telemetry.local_id(), local_id);
    ASSERT_TRUE(fatal_crash_telemetry.has_timestamp_us());
    ASSERT_EQ(
        fatal_crash_telemetry.timestamp_us(),
        FatalCrashEventsObserver::ConvertTimeToMicroseconds(capture_time));
  }

  // Wait for the given fatal crash event being skipped.
  FatalCrashEventsObserver::LocalIdEntry WaitForSkippedFatalCrashEvent(
      std::string_view local_id,
      base::Time capture_time,
      FatalCrashEventsObserver& fatal_crash_observer) {
    base::test::TestFuture<FatalCrashEventsObserver::LocalIdEntry> result;
    fatal_crash_observer.SetSkippedCrashCallback(result.GetRepeatingCallback());

    auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
    crash_event_info->local_id = local_id;
    crash_event_info->capture_time = capture_time;
    FakeCrosHealthd::Get()->EmitEventForCategory(
        EventCategoryEnum::kCrash,
        EventInfo::NewCrashEventInfo(std::move(crash_event_info)));

    return result.Take();
  }
};

class FatalCrashEventsObserverReportedLocalIdsTest
    : public FatalCrashEventsObserverReportedLocalIdsTestBase,
      public ::testing::WithParamInterface</*reload=*/bool> {
 public:
  FatalCrashEventsObserverReportedLocalIdsTest(
      const FatalCrashEventsObserverReportedLocalIdsTest&) = delete;
  FatalCrashEventsObserverReportedLocalIdsTest& operator=(
      const FatalCrashEventsObserverReportedLocalIdsTest&) = delete;

 protected:
  FatalCrashEventsObserverReportedLocalIdsTest() = default;
  ~FatalCrashEventsObserverReportedLocalIdsTest() override = default;

  // Whether the fatal crash events observer should reload to simulate user
  // restarting ash.
  bool reload() const { return GetParam(); }
};

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       RepeatedLocalIDNotReported) {
  auto fatal_crash_events_observer = CreateAndEnableFatalCrashEventsObserver();
  CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                        *fatal_crash_events_observer);
  if (reload()) {
    fatal_crash_events_observer = CreateAndEnableFatalCrashEventsObserver();
  }
  const auto local_id_entry = WaitForSkippedFatalCrashEvent(
      /*local_id=*/kLocalId,
      /*capture_time=*/kCaptureTime, *fatal_crash_events_observer);
  EXPECT_EQ(local_id_entry.local_id, kLocalId);
  EXPECT_EQ(local_id_entry.capture_timestamp_us,
            FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTime));
}

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       UnwritableSaveFileRepeatedLocalIdNotReportedIfNotReloaded) {
  // Even if save file is unwritable, the same observer should still skip the
  // unuploaded crash with the same local ID if the user does not restart ash,
  // while it is outside of our control if ash has been restarted.
  ASSERT_TRUE(base::MakeFileUnwritable(GetSaveFilePath().DirName()));
  auto fatal_crash_events_observer = CreateAndEnableFatalCrashEventsObserver();
  CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                        *fatal_crash_events_observer);
  if (reload()) {
    // As a sanity test, if the observer is reloaded, then the repeated local ID
    // would not lead to a skipped crash.
    fatal_crash_events_observer = CreateAndEnableFatalCrashEventsObserver();
    // We are uninterested in the crash itself since this is a sanity test, only
    // need to know that a new crash is reported.
    CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                          *fatal_crash_events_observer);
  } else {
    const auto local_id_entry = WaitForSkippedFatalCrashEvent(
        /*local_id=*/kLocalId,
        /*capture_time=*/kCaptureTime, *fatal_crash_events_observer);
    EXPECT_EQ(local_id_entry.local_id, kLocalId);
    EXPECT_EQ(
        local_id_entry.capture_timestamp_us,
        FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTime));
  }
}

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       TooManySavedEarlierSkippedLaterReported) {
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  for (size_t i = 0u; i < kMaxNumOfLocalIds; ++i) {
    std::ostringstream ss;
    ss << kLocalId << i;
    CreateFatalCrashEvent(/*local_id=*/ss.str(), /*capture_time=*/kCaptureTime,
                          *fatal_crash_events_observer, &result_metric_data);
  }

  if (reload()) {
    fatal_crash_events_observer =
        CreateAndEnableFatalCrashEventsObserver(&result_metric_data);
  }

  // Crashes with earlier or the same timestamp are skipped.
  auto local_id_entry = WaitForSkippedFatalCrashEvent(
      /*local_id=*/kLocalIdEarly,
      /*capture_time=*/kCaptureTimeEarly, *fatal_crash_events_observer);
  EXPECT_EQ(local_id_entry.local_id, kLocalIdEarly);
  EXPECT_EQ(
      local_id_entry.capture_timestamp_us,
      FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeEarly));
  local_id_entry = WaitForSkippedFatalCrashEvent(/*local_id=*/kLocalId,
                                                 /*capture_time=*/kCaptureTime,
                                                 *fatal_crash_events_observer);
  EXPECT_EQ(local_id_entry.local_id, kLocalId);
  EXPECT_EQ(local_id_entry.capture_timestamp_us,
            FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTime));

  // Crashes with later timestamps are reported.
  auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
  crash_event_info->local_id = kLocalIdLate;
  crash_event_info->capture_time = kCaptureTimeLate;
  const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
      std::move(crash_event_info), fatal_crash_events_observer.get(),
      &result_metric_data);
  ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
  EXPECT_EQ(fatal_crash_telemetry.local_id(), kLocalIdLate);
  ASSERT_TRUE(fatal_crash_telemetry.has_timestamp_us());
  EXPECT_EQ(
      fatal_crash_telemetry.timestamp_us(),
      FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeLate));
}

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       RepeatedLocalIDReportedIfFirstTimeIsInterrupted) {
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);
  // Simulate the thread is interrupted after event observed callback is called.
  fatal_crash_test_environment_.SetInterruptedAfterEventObserved(
      *fatal_crash_events_observer, /*interrupted_after_event_observed=*/true);
  CreateFatalCrashEvent(
      /*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
      *fatal_crash_events_observer, &result_metric_data);
  if (reload()) {
    fatal_crash_events_observer =
        CreateAndEnableFatalCrashEventsObserver(&result_metric_data);
  }

  // Now back to what production code does.
  fatal_crash_test_environment_.SetInterruptedAfterEventObserved(
      *fatal_crash_events_observer, /*interrupted_after_event_observed=*/false);

  // Event with the same local ID is reported again.
  auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
  crash_event_info->local_id = kLocalId;
  crash_event_info->capture_time = kCaptureTime;
  const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
      std::move(crash_event_info), fatal_crash_events_observer.get(),
      &result_metric_data);
  ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
  EXPECT_EQ(fatal_crash_telemetry.local_id(), kLocalId);
  ASSERT_TRUE(fatal_crash_telemetry.has_timestamp_us());
  EXPECT_EQ(fatal_crash_telemetry.timestamp_us(),
            FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTime));
}

// TODO(b/266018440): After implementing the logic that controls whether an
// uploaded crash should be reported (which would include the logic to remove
// crashes from saved unuploaded crashes), test here that the earliest crash has
// been removed after a sufficient amount of later crashes are reported.

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverReportedLocalIdsTests,
    FatalCrashEventsObserverReportedLocalIdsTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<
        FatalCrashEventsObserverReportedLocalIdsTest::ParamType>& info) {
      return info.param ? "reload" : "same_session";
    });

struct FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileCase {
  std::string name;
  std::string save_file_content;
};

class FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest
    : public FatalCrashEventsObserverReportedLocalIdsTestBase,
      public ::testing::WithParamInterface<
          FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileCase> {
 public:
  FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest(
      const FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest&) =
      delete;
  FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest& operator=(
      const FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest&) =
      delete;

 protected:
  static constexpr base::Time kCaptureTimeZero{base::Time::FromTimeT(0)};

  FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest() = default;
  ~FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest() override =
      default;

  const std::string& save_file_content() const {
    return GetParam().save_file_content;
  }
};

TEST_P(FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest,
       CorruptFileIgnored) {
  // When the first line is corrupt, nothing should be loaded.
  ASSERT_TRUE(base::WriteFile(GetSaveFilePath(), save_file_content()));
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  // Verify that no crash is loaded by creating kMaxNumOfLocalIds number of
  // zero timestamp crashes. If one crash is loaded, then it would fail to
  // load one of them.
  for (size_t i = 0u; i < kMaxNumOfLocalIds; ++i) {
    std::ostringstream ss;
    ss << kLocalId << i;
    auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
    crash_event_info->local_id = ss.str();
    crash_event_info->capture_time = kCaptureTimeZero;
    const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
        std::move(crash_event_info), fatal_crash_events_observer.get(),
        &result_metric_data);
    ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
    EXPECT_EQ(fatal_crash_telemetry.local_id(), ss.str());
    ASSERT_TRUE(fatal_crash_telemetry.has_timestamp_us());
    EXPECT_EQ(
        fatal_crash_telemetry.timestamp_us(),
        FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeZero));
  }

  // The next crash event should still be skipped. Need this line for testing
  // corrupt files because we need to ensure the negative timestamped crashes
  // are not loaded.
  const auto local_id_entry = WaitForSkippedFatalCrashEvent(
      /*local_id=*/kLocalId,
      /*capture_time=*/kCaptureTimeZero, *fatal_crash_events_observer);
  EXPECT_EQ(local_id_entry.local_id, kLocalId);
  EXPECT_EQ(
      local_id_entry.capture_timestamp_us,
      FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeZero));
}

TEST_F(FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest,
       SecondLineIsCorrupt) {
  // When the second line is corrupt, the first line should still be loaded.
  // No need to run through all parameterized corrupt line, as the focus here
  // is that only the first line is loaded.
  ASSERT_TRUE(
      base::WriteFile(GetSaveFilePath(), "good_line,100\ncorrupt_line"));
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  for (size_t i = 0u; i < kMaxNumOfLocalIds - 1; ++i) {
    std::ostringstream ss;
    ss << kLocalId << i;
    CreateFatalCrashEvent(/*local_id=*/ss.str(),
                          /*capture_time=*/kCaptureTimeZero,
                          *fatal_crash_events_observer, &result_metric_data);
  }

  // Because one good line is still parsed and loaded, the
  // kMaxNumOfLocalIds'th crash with the zero capture time would be skipped.
  const auto local_id_entry = WaitForSkippedFatalCrashEvent(
      /*local_id=*/kLocalId,
      /*capture_time=*/kCaptureTimeZero, *fatal_crash_events_observer);
}

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTests,
    FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest,
    ::testing::ValuesIn(
        std::vector<
            FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileCase>{
            {.name = "empty", .save_file_content = ""},
            {.name = "one_column", .save_file_content = "first_column"},
            {.name = "three_columns",
             .save_file_content = "first_column,second_column,third_column"},
            {.name = "unparsable_timestamp",
             .save_file_content = "local ID,not_a_number"},
            {.name = "negative_timestamp",
             .save_file_content = "local ID,-100"},
            // When a corrupt line is followed by a good line, the good line
            // is also not parsed, because parsing would stop at any line
            // found corrupted.
            {.name = "corrupt_line_followed_by_a_good_line",
             .save_file_content = "corrupt_line\ngood_line,100"}}),
    [](const testing::TestParamInfo<
        FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest::ParamType>&
           info) { return info.param.name; });
}  // namespace
}  // namespace reporting
