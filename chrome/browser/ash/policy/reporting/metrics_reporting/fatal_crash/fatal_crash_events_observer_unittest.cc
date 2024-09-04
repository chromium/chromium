// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include <atomic>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/chrome_fatal_crash_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_reported_local_id_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_settings_for_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_test_util.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_uploaded_crash_info_manager.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

using ::ash::cros_healthd::FakeCrosHealthd;
using ::ash::cros_healthd::mojom::CrashEventInfo;
using ::ash::cros_healthd::mojom::CrashEventInfoPtr;
using ::ash::cros_healthd::mojom::CrashUploadInfo;
using ::ash::cros_healthd::mojom::EventCategoryEnum;
using ::ash::cros_healthd::mojom::EventInfo;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::SizeIs;
using ::testing::StrEq;

namespace {
// RAII class to interrupt after event is observed.
class ScopedInterruptedAfterEventObserved {
 public:
  // `observer` must remain alive when this object destructs.
  explicit ScopedInterruptedAfterEventObserved(
      FatalCrashEventsObserver& observer)
      : observer_(&observer) {
    FatalCrashEventsObserver::TestEnvironment::GetTestSettings(*observer_)
        .interrupted_after_event_observed = true;
  }

  virtual ~ScopedInterruptedAfterEventObserved() {
    FatalCrashEventsObserver::TestEnvironment::GetTestSettings(*observer_)
        .interrupted_after_event_observed = false;
  }

  ScopedInterruptedAfterEventObserved(
      const ScopedInterruptedAfterEventObserved&) = delete;
  ScopedInterruptedAfterEventObserved& operator=(
      const ScopedInterruptedAfterEventObserved&) = delete;

  // The move constructor and assignment are currently unused, but there's
  // no reason to not support them as they are standard in scoped classes.
  ScopedInterruptedAfterEventObserved(ScopedInterruptedAfterEventObserved&&) =
      default;
  ScopedInterruptedAfterEventObserved& operator=(
      ScopedInterruptedAfterEventObserved&&) = default;

 private:
  raw_ptr<FatalCrashEventsObserver> observer_;
};

}  // namespace

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

  // Create a new `CrashEventInfo` object that respects the `is_uploaded`
  // param.
  static CrashEventInfoPtr NewCrashEventInfo(bool is_uploaded) {
    auto crash_event_info = CrashEventInfo::New();
    // Only allowed crash types are reported. Make "kernel" the default type for
    // test purposes.
    crash_event_info->crash_type = CrashEventInfo::CrashType::kKernel;

    if (is_uploaded) {
      crash_event_info->upload_info = CrashUploadInfo::New();
      crash_event_info->upload_info->crash_report_id = kCrashReportId;
      // The default zero time is earlier than the UNIX epoch.
      crash_event_info->upload_info->creation_time = base::Time::UnixEpoch();
    }

    return crash_event_info;
  }

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
      base::test::TestFuture<MetricData>* test_event = nullptr,
      CrashEventInfo::CrashType crash_type =
          CrashEventInfo::CrashType::kDefaultValue) const {
    auto observer =
        fatal_crash_test_environment_.CreateFatalCrashEventsObserver(
            /*reported_local_id_io_task_runner=*/nullptr,
            /*uploaded_crash_info_io_task_runner=*/nullptr, crash_type);
    observer->SetReportingEnabled(true);
    if (test_event) {
      observer->SetOnEventObservedCallback(test_event->GetRepeatingCallback());
    }
    return observer;
  }

  // Recreate a `FatalCrashEventsObserver` object and enables reporting. It
  // optionally sets the OnEventObserved callback if test_event is provided. It
  // ensures that the existing `FatalCrashEventsObserver` object is destroyed
  // and all its IO tasks are executed first before creating the new object.
  void RecreateAndEnableFatalCrashEventsObserver(
      std::unique_ptr<FatalCrashEventsObserver>& observer,
      base::test::TestFuture<MetricData>* test_event = nullptr) const {
    // Clear the current sequence as IO tasks may be posted.
    base::RunLoop().RunUntilIdle();
    // Make sure all save file changes in the observer instance to be destroyed
    // are finished. Otherwise, the new observer instance may fail to read the
    // save file.
    FatalCrashEventsObserver::TestEnvironment::FlushIoTasks(*observer);
    // Don't assign directly as assignment will cause the current observer to be
    // destroyed after the new observer is created. Explicitly call `reset` here
    // to ensure the current observer is destroyed first.
    observer.reset();
    observer = CreateAndEnableFatalCrashEventsObserver(test_event);
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
  //
  // Also performs some simple verifications, such as event types and the
  // existence of fatal crash telemetry in the resulted `MetricData`.
  FatalCrashTelemetry WaitForFatalCrashTelemetry(
      CrashEventInfoPtr crash_event_info,
      FatalCrashEventsObserver* fatal_crash_events_observer = nullptr,
      base::test::TestFuture<MetricData>* result_metric_data = nullptr) const {
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

    EXPECT_TRUE(metric_data.has_event_data());
    EXPECT_TRUE(metric_data.event_data().has_type());
    EXPECT_EQ(metric_data.event_data().type(), MetricEventType::FATAL_CRASH);

    EXPECT_TRUE(metric_data.has_telemetry_data());
    EXPECT_TRUE(metric_data.telemetry_data().has_fatal_crash_telemetry());
    return std::move(metric_data.telemetry_data().fatal_crash_telemetry());
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

// Tests `FatalCrashEventsObserver` passing the type field with `type` and
// `uploaded` being parameterized.
class FatalCrashEventsObserverTypeFieldTest
    : public FatalCrashEventsObserverTestBase,
      public ::testing::WithParamInterface<
          std::tuple</*type=*/CrashEventInfo::CrashType, /*uploaded=*/bool>> {
 public:
  FatalCrashEventsObserverTypeFieldTest(
      const FatalCrashEventsObserverTypeFieldTest&) = delete;
  FatalCrashEventsObserverTypeFieldTest& operator=(
      const FatalCrashEventsObserverTypeFieldTest&) = delete;

 protected:
  FatalCrashEventsObserverTypeFieldTest() = default;
  ~FatalCrashEventsObserverTypeFieldTest() override = default;

  CrashEventInfo::CrashType type() const { return std::get<0>(GetParam()); }
  bool is_uploaded() const { return std::get<1>(GetParam()); }
};

TEST_P(FatalCrashEventsObserverTypeFieldTest, FieldTypePassedThrough) {
  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  crash_event_info->crash_type = type();
  auto observer = CreateAndEnableFatalCrashEventsObserver(
      /*test_event=*/nullptr, crash_event_info->crash_type);

  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info), observer.get(),
                                 /*result_metric_data=*/nullptr);

  ASSERT_TRUE(fatal_crash_telemetry.has_type());
  FatalCrashTelemetry::CrashType expected_crash_type;
  switch (type()) {
    case CrashEventInfo::CrashType::kKernel:
      expected_crash_type = FatalCrashTelemetry::CRASH_TYPE_KERNEL;
      break;
    case CrashEventInfo::CrashType::kEmbeddedController:
      expected_crash_type = FatalCrashTelemetry::CRASH_TYPE_EMBEDDED_CONTROLLER;
      break;
    case CrashEventInfo::CrashType::kChrome:
      expected_crash_type = FatalCrashTelemetry::CRASH_TYPE_CHROME;
      break;
    default:  // Crash types that are not tested but should be tested.
      NOTREACHED() << "Encountered untested crash type " << type();
  }
  EXPECT_EQ(fatal_crash_telemetry.type(), expected_crash_type);
}

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverTypeFieldTests,
    FatalCrashEventsObserverTypeFieldTest,
    ::testing::Combine(
        ::testing::ValuesIn(
            FatalCrashEventsObserver::TestEnvironment::GetAllowedCrashTypes()),
        ::testing::Bool()),
    [](const testing::TestParamInfo<
        FatalCrashEventsObserverTypeFieldTest::ParamType>& info) {
      std::ostringstream ss;
      ss << "type_" << std::get<0>(info.param) << '_'
         << (std::get<1>(info.param) ? "uploaded" : "unuploaded");
      return ss.str();
    });

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

TEST_F(FatalCrashEventsObserverTest,
       FieldBeenReportedWithoutCrashReportIdUnsetIfUnuploaded) {
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(NewCrashEventInfo(/*is_uploaded=*/false));
  EXPECT_FALSE(
      fatal_crash_telemetry.has_been_reported_without_crash_report_id());
}

TEST_F(
    FatalCrashEventsObserverTest,
    FieldBeenReportedWithoutCrashReportIdIsFalseIfUploadedNotPreviouslyReported) {
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(NewCrashEventInfo(/*is_uploaded=*/true));
  ASSERT_TRUE(
      fatal_crash_telemetry.has_been_reported_without_crash_report_id());
  EXPECT_FALSE(fatal_crash_telemetry.been_reported_without_crash_report_id());
}

TEST_F(
    FatalCrashEventsObserverTest,
    FieldBeenReportedWithoutCrashReportIdIsTrueIfUploadedAndPreviouslyReported) {
  static constexpr std::string_view kLocalId = "a local ID";

  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  // First an unuploaded crash with a set local ID.
  auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
  crash_event_info->local_id = kLocalId;
  std::ignore = WaitForFatalCrashTelemetry(std::move(crash_event_info),
                                           fatal_crash_events_observer.get(),
                                           &result_metric_data);

  // Second an uploaded crash with the same local ID.
  crash_event_info = NewCrashEventInfo(/*is_uploaded=*/true);
  crash_event_info->local_id = kLocalId;
  const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
      std::move(crash_event_info), fatal_crash_events_observer.get(),
      &result_metric_data);
  ASSERT_TRUE(
      fatal_crash_telemetry.has_been_reported_without_crash_report_id());
  EXPECT_TRUE(fatal_crash_telemetry.been_reported_without_crash_report_id());
}

TEST_P(FatalCrashEventsObserverTest, FieldUserEmailFilledIfAffiliated) {
  SimulateUserLogin(kUserEmail, user_manager::UserType::kRegular,
                    /*is_user_affiliated=*/true);
  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));

  ASSERT_TRUE(fatal_crash_telemetry.has_affiliated_user());
  ASSERT_TRUE(fatal_crash_telemetry.affiliated_user().has_user_email());
  EXPECT_EQ(fatal_crash_telemetry.affiliated_user().user_email(), kUserEmail);
}

TEST_P(FatalCrashEventsObserverTest, FieldUserEmailAbsentIfUnaffiliated) {
  SimulateUserLogin(kUserEmail, user_manager::UserType::kRegular,
                    /*is_user_affiliated=*/false);
  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  const auto fatal_crash_telemetry =
      WaitForFatalCrashTelemetry(std::move(crash_event_info));
  EXPECT_FALSE(fatal_crash_telemetry.has_affiliated_user());
}

TEST_P(FatalCrashEventsObserverTest, FieldUnknownTypeSkipped) {
  auto fatal_crash_events_observer = CreateAndEnableFatalCrashEventsObserver();
  DCHECK_CALLED_ON_VALID_SEQUENCE(
      FatalCrashEventsObserver::TestEnvironment::GetTestSettings(
          *fatal_crash_events_observer)
          .sequence_checker);

  base::test::TestFuture<CrashEventInfo::CrashType> result;
  FatalCrashEventsObserver::TestEnvironment::GetTestSettings(
      *fatal_crash_events_observer)
      .skipped_uninteresting_crash_type_callback =
      result.GetRepeatingCallback();

  auto crash_event_info = NewCrashEventInfo(is_uploaded());
  crash_event_info->crash_type = CrashEventInfo::CrashType::kUnknown;
  FakeCrosHealthd::Get()->EmitEventForCategory(
      EventCategoryEnum::kCrash,
      EventInfo::NewCrashEventInfo(std::move(crash_event_info)));

  EXPECT_EQ(result.Take(), CrashEventInfo::CrashType::kUnknown);
}

TEST_P(FatalCrashEventsObserverTest, ObserveMultipleEvents) {
  // The observer is capable of observing multiple events.
  base::test::TestFuture<MetricData> test_event;
  auto observer = CreateAndEnableFatalCrashEventsObserver(&test_event);

  for (int i = 0; i < 10; ++i) {
    const auto local_id = base::NumberToString(i);
    auto crash_event_info = NewCrashEventInfo(is_uploaded());
    crash_event_info->local_id = local_id;
    if (is_uploaded()) {
      // Incremental offset, otherwise the later uploaded crashes would not be
      // reported.
      crash_event_info->upload_info->offset = i;
    }
    const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
        std::move(crash_event_info), observer.get(), &test_event);
    ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
    EXPECT_EQ(fatal_crash_telemetry.local_id(), local_id);
  }
}

TEST_P(FatalCrashEventsObserverTest, SlowFileLoadingFieldsPassedThrough) {
  // Test that fields are passed through for crash events that either:
  //   1. come before save files are loaded, or
  //   2. before all crashes queued before save files are loaded.

  // Because FakeCrosHealthd::Get()->EmitEventForCategory() causes tasks on the
  // current thread to be processed, for this test alone, we call
  // FatalCrashEventsObserver::OnEvent directly to emulate the effect of
  // FakeCrosHealthd::Get()->EmitEventForCategory().

  // Need 4 crash events to work around limitations in manipulating tasks in a
  // sequence.
  static constexpr size_t kNumCrashes = 4;
  static constexpr std::array<std::string_view, kNumCrashes> kLocalIds = {
      "First local ID", "Second local ID", "Third Local ID", "Fourth Local ID"};

  std::array<CrashEventInfoPtr, kNumCrashes> crash_event_infos = {
      NewCrashEventInfo(is_uploaded()), NewCrashEventInfo(is_uploaded()),
      NewCrashEventInfo(is_uploaded()), NewCrashEventInfo(is_uploaded())};
  for (size_t i = 0; i < crash_event_infos.size(); ++i) {
    // Test the local ID field sufficient.
    crash_event_infos[i]->local_id = kLocalIds[i];

    // Make uploaded crashes have increasing offset, which is the most practical
    // scenario.
    if (crash_event_infos[i]->upload_info) {
      crash_event_infos[i]->upload_info->offset = i;
    }
  }

  const auto io_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  // Block the IO thread.
  FatalCrashEventsObserver::TestEnvironment::SequenceBlocker sequence_blocker(
      io_task_runner);

  // Create and set up the observer object.
  auto observer = fatal_crash_test_environment_.CreateFatalCrashEventsObserver(
      /*reported_local_id_io_task_runner=*/is_uploaded() ? nullptr
                                                         : io_task_runner,
      /*uploaded_crash_info_io_task_runner=*/is_uploaded() ? io_task_runner
                                                           : nullptr);
  DCHECK_CALLED_ON_VALID_SEQUENCE(
      FatalCrashEventsObserver::TestEnvironment::GetTestSettings(*observer)
          .sequence_checker);
  observer->SetReportingEnabled(true);
  // Not using `TestFuture`, because it can only accept one value at a time and
  // generates an error if another values comes in before the first value is
  // taken. Due to the racing of the task sequence in unit tests, pushing
  // results to a vector would not be flaky.
  std::vector<MetricData> results;
  results.reserve(4u);
  observer->SetOnEventObservedCallback(base::BindRepeating(
      [](std::vector<MetricData>* results,
         scoped_refptr<base::SequencedTaskRunner> main_task_runner,
         MetricData metric_data) {
        ASSERT_THAT(base::SequencedTaskRunner::GetCurrentDefault(),
                    Eq(main_task_runner));
        results->push_back(std::move(metric_data));
      },
      &results, base::SequencedTaskRunner::GetCurrentDefault()));
  base::test::TestFuture<CrashEventInfoPtr> queued_crash_event_result;
  FatalCrashEventsObserver::TestEnvironment::GetTestSettings(*observer)
      .event_collected_before_save_files_loaded_callback =
      queued_crash_event_result.GetRepeatingCallback();

  // Emit the first 3 events before the save file is loaded. The event is queued
  // and saved in RAM.
  for (size_t i = 0; i < 3u; ++i) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FatalCrashEventsObserver::OnEvent,
                                  observer->weak_factory_.GetWeakPtr(),
                                  EventInfo::NewCrashEventInfo(
                                      std::move(crash_event_infos[i]))));
    // Sanity check to ensure that the crash event is indeed queued.
    EXPECT_THAT(queued_crash_event_result.Take()->local_id, Eq(kLocalIds[i]));
  }

  // Unblock the IO, flush the IO (thus save files are loaded), and emit the
  // fourth event. Because the third event has not been processed yet when
  // `OnEvent` for the fourth event is called, the fourth event is also expected
  // to be queued up.
  ASSERT_TRUE(!observer->AreSaveFilesLoaded())
      << "Internal error: Save files are loaded even task thread is blocked";
  sequence_blocker.Unblock();
  FatalCrashEventsObserver::TestEnvironment::
      FlushTaskRunnerWithCurrentSequenceBlocked(io_task_runner);
  ASSERT_TRUE(observer->AreSaveFilesLoaded())
      << "Internal error: Flushing IO tasks does not finish loading save files";
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FatalCrashEventsObserver::OnEvent,
                                observer->weak_factory_.GetWeakPtr(),
                                EventInfo::NewCrashEventInfo(
                                    std::move(crash_event_infos[3]))));
  // Flushing IO tasks causes the first ProcessEventsBeforeSaveFilesLoaded task
  // (which has filled result_metric_data) executed and the second
  // ProcessEventsBeforeSaveFilesLoaded task left in the sequence. Therefore,
  // the current sequence contains the second ProcessEventsBeforeSaveFilesLoaded
  // task followed by one OnEvent task.
  // Sanity check to ensure that the crash event is indeed queued.
  EXPECT_THAT(queued_crash_event_result.Take()->local_id, Eq(kLocalIds[3]));

  // All crash events should be available in order, and the event collected call
  // back should never be called from this point on.
  FatalCrashEventsObserver::TestEnvironment::GetTestSettings(*observer)
      .event_collected_before_save_files_loaded_callback =
      base::BindRepeating([](CrashEventInfoPtr crash_event_info) {
        // Sanity check to ensure that no more crash event is queued.
        EXPECT_FALSE(true) << "Found unexpected queued crash event: "
                           << crash_event_info->local_id;
      });
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(results, SizeIs(4u));
  for (size_t i = 0; i < results.size(); ++i) {
    const auto& metric_data = results[i];
    ASSERT_TRUE(metric_data.has_telemetry_data());
    ASSERT_TRUE(metric_data.telemetry_data().has_fatal_crash_telemetry());
    const auto& fatal_crash_telemetry =
        metric_data.telemetry_data().fatal_crash_telemetry();

    ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
    EXPECT_EQ(fatal_crash_telemetry.local_id(), kLocalIds[i]);
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
      {.user_type = user_manager::UserType::kChild,
       .session_type = FatalCrashTelemetry::SESSION_TYPE_CHILD},
      {.user_type = user_manager::UserType::kGuest,
       .session_type = FatalCrashTelemetry::SESSION_TYPE_GUEST}};

  for (size_t i = 0; i < std::size(kSessionTypes); ++i) {
    SimulateUserLogin(kUserEmail, kSessionTypes[i].user_type,
                      is_user_affiliated());
    auto crash_event_info = NewCrashEventInfo(is_uploaded());
    if (is_uploaded()) {
      // Incremental offset, otherwise the later uploaded crashes would not be
      // reported.
      crash_event_info->upload_info->offset = i;
    }
    // Crash with the same local ID would be ignored, assign a unique local ID
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
  using ShouldReportResult =
      FatalCrashEventsObserver::TestEnvironment::ShouldReportResult;

  // The maximum number of local IDs to save.
  static constexpr size_t kMaxNumOfLocalIds{
      FatalCrashEventsObserver::TestEnvironment::kMaxNumOfLocalIds};
  // The maximum size of the priority queue before reconstructing it.
  static constexpr size_t kMaxSizeOfLocalIdEntryQueue{
      FatalCrashEventsObserver::TestEnvironment::kMaxSizeOfLocalIdEntryQueue};
  static constexpr const char* kUmaUnuploadedCrashShouldNotReportReason{
      FatalCrashEventsObserver::kUmaUnuploadedCrashShouldNotReportReason};
  static constexpr std::string_view kLocalId = "local ID";
  static constexpr base::Time kCaptureTime = base::Time::FromTimeT(14);
  static constexpr std::string_view kLocalIdEarly = "local ID Early";
  static constexpr base::Time kCaptureTimeEarly = base::Time::FromTimeT(10);
  static constexpr std::string_view kLocalIdLate = "local ID Late";
  static constexpr base::Time kCaptureTimeLate = base::Time::FromTimeT(20);

  FatalCrashEventsObserverReportedLocalIdsTestBase() = default;
  ~FatalCrashEventsObserverReportedLocalIdsTestBase() override = default;

  // Gets the path to the save file.
  base::FilePath GetSaveFilePath() const {
    return fatal_crash_test_environment_.GetReportedLocalIdSaveFilePath();
  }

  // Generates an uninteresting fatal crash event to alter the observer's state
  // in preparation for the test.
  void CreateFatalCrashEvent(
      std::string_view local_id,
      base::Time capture_time,
      FatalCrashEventsObserver& fatal_crash_observer,
      base::test::TestFuture<MetricData>* test_event = nullptr,
      bool is_uploaded = false) const {
    static uint64_t offset = 0u;

    auto crash_event_info = NewCrashEventInfo(is_uploaded);
    crash_event_info->local_id = local_id;
    crash_event_info->capture_time = capture_time;
    if (is_uploaded) {
      // Keep offset increasing so that uploaded crash event would not be
      // blocked because the offset is smaller than previous. Not allowing fine
      // tuning offset here because the test focuses on
      // `ReportedLocalIdManager`, not the role offset plays in uploaded
      // crashes.
      crash_event_info->upload_info->offset = offset++;
    }

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
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        FatalCrashEventsObserver::TestEnvironment::GetTestSettings(
            fatal_crash_observer)
            .sequence_checker);

    base::test::TestFuture<FatalCrashEventsObserver::LocalIdEntry> result;
    FatalCrashEventsObserver::TestEnvironment::GetTestSettings(
        fatal_crash_observer)
        .skipped_unuploaded_crash_callback = result.GetRepeatingCallback();

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

  // Creates a `FatalCrashEventsObserver` object with the number of saved local
  // IDs maximized.
  std::tuple<std::unique_ptr<FatalCrashEventsObserver>,
             std::unique_ptr<base::test::TestFuture<MetricData>>>
  CreateFatalCrashEventsObserverFilledWithMaxNumberOfSavedLocalIds() const {
    auto result_metric_data =
        std::make_unique<base::test::TestFuture<MetricData>>();
    auto fatal_crash_events_observer =
        CreateAndEnableFatalCrashEventsObserver(result_metric_data.get());

    for (size_t i = 0u; i < kMaxNumOfLocalIds; ++i) {
      std::ostringstream ss;
      ss << kLocalId << i;
      CreateFatalCrashEvent(/*local_id=*/ss.str(),
                            /*capture_time=*/kCaptureTime,
                            *fatal_crash_events_observer,
                            result_metric_data.get());
    }

    return std::make_tuple(std::move(fatal_crash_events_observer),
                           std::move(result_metric_data));
  }
};

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       RepeatedLocalIDNotReported) {
  auto fatal_crash_events_observer = CreateAndEnableFatalCrashEventsObserver();
  CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                        *fatal_crash_events_observer);
  if (reload()) {
    RecreateAndEnableFatalCrashEventsObserver(fatal_crash_events_observer);
  }

  base::HistogramTester histogram_tester;
  const auto local_id_entry = WaitForSkippedFatalCrashEvent(
      /*local_id=*/kLocalId,
      /*capture_time=*/kCaptureTime, *fatal_crash_events_observer);
  EXPECT_EQ(local_id_entry.local_id, kLocalId);
  EXPECT_EQ(local_id_entry.capture_timestamp_us,
            FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTime));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kUmaUnuploadedCrashShouldNotReportReason),
      base::BucketsAre(base::Bucket(ShouldReportResult::kHasBeenReported, 1)));
}

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       UncreatableSaveFileRepeatedLocalIdNotReportedIfNotReloaded) {
  // Even if save file can't be created, the same observer should still skip the
  // unuploaded crash with the same local ID if the user does not restart ash,
  // while it is outside of our control if ash has been restarted.
  ASSERT_TRUE(base::MakeFileUnwritable(GetSaveFilePath().DirName()));
  auto fatal_crash_events_observer = CreateAndEnableFatalCrashEventsObserver();
  CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                        *fatal_crash_events_observer);
  if (reload()) {
    // As a sanity test, if the observer is reloaded, then the repeated local ID
    // would not lead to a skipped crash.
    RecreateAndEnableFatalCrashEventsObserver(fatal_crash_events_observer);
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
       TooManySavedEarlierCrashesSkipped) {
  auto [fatal_crash_events_observer, result_metric_data] =
      CreateFatalCrashEventsObserverFilledWithMaxNumberOfSavedLocalIds();

  if (reload()) {
    RecreateAndEnableFatalCrashEventsObserver(fatal_crash_events_observer,
                                              result_metric_data.get());
  }

  // Crashes with an earlier timestamp are skipped. Repeat twice for robustness
  // -- after the first crash is not reported, the second crash still should not
  // be reported.
  for (int i = 0; i < 2; ++i) {
    base::HistogramTester histogram_tester;
    std::ostringstream ss;
    ss << kLocalIdEarly << i;
    const auto local_id_entry = WaitForSkippedFatalCrashEvent(
        /*local_id=*/ss.str(),
        /*capture_time=*/kCaptureTimeEarly, *fatal_crash_events_observer);
    EXPECT_EQ(local_id_entry.local_id, ss.str());
    EXPECT_EQ(
        local_id_entry.capture_timestamp_us,
        FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeEarly));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            kUmaUnuploadedCrashShouldNotReportReason),
        base::BucketsAre(base::Bucket(
            ShouldReportResult::kCrashTooOldAndMaxNumOfSavedLocalIdsReached,
            1)));
  }
}

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       TooManySavedSameTimeCrashesSkipped) {
  auto [fatal_crash_events_observer, result_metric_data] =
      CreateFatalCrashEventsObserverFilledWithMaxNumberOfSavedLocalIds();

  if (reload()) {
    RecreateAndEnableFatalCrashEventsObserver(fatal_crash_events_observer,
                                              result_metric_data.get());
  }

  // Crashes with the same timestamp are skipped. Repeat twice for robustness --
  // after the first crash is not reported, the second crash still should not be
  // reported.
  for (int i = 0; i < 2; ++i) {
    base::HistogramTester histogram_tester;
    std::ostringstream ss;
    // Use -(i + 1) here to avoid same local IDs with the one already saved by
    // the observer.
    ss << kLocalId << -(i + 1);
    const auto local_id_entry = WaitForSkippedFatalCrashEvent(
        /*local_id=*/ss.str(),
        /*capture_time=*/kCaptureTime, *fatal_crash_events_observer);
    EXPECT_EQ(local_id_entry.local_id, ss.str());
    EXPECT_EQ(
        local_id_entry.capture_timestamp_us,
        FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTime));
    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            kUmaUnuploadedCrashShouldNotReportReason),
        base::BucketsAre(base::Bucket(
            ShouldReportResult::kCrashTooOldAndMaxNumOfSavedLocalIdsReached,
            1)));
  }
}

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       TooManySavedLaterCrashReported) {
  auto [fatal_crash_events_observer, result_metric_data] =
      CreateFatalCrashEventsObserverFilledWithMaxNumberOfSavedLocalIds();

  if (reload()) {
    RecreateAndEnableFatalCrashEventsObserver(fatal_crash_events_observer,
                                              result_metric_data.get());
  }

  // Crash with later timestamps are reported.
  base::HistogramTester histogram_tester;
  auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
  crash_event_info->local_id = kLocalIdLate;
  crash_event_info->capture_time = kCaptureTimeLate;
  const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
      std::move(crash_event_info), fatal_crash_events_observer.get(),
      result_metric_data.get());
  ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
  EXPECT_EQ(fatal_crash_telemetry.local_id(), kLocalIdLate);
  ASSERT_TRUE(fatal_crash_telemetry.has_timestamp_us());
  EXPECT_EQ(
      fatal_crash_telemetry.timestamp_us(),
      FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeLate));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kUmaUnuploadedCrashShouldNotReportReason),
      base::BucketsAre(base::Bucket(ShouldReportResult::kYes, 1)));
}

TEST_P(FatalCrashEventsObserverReportedLocalIdsTest,
       CrashReportedAsUploadedIsRemoved) {
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  // Same crash, first reported as unuploaded, then reported again as uploaded.
  CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                        *fatal_crash_events_observer, &result_metric_data,
                        /*is_uploaded=*/false);
  CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                        *fatal_crash_events_observer, &result_metric_data,
                        /*is_uploaded=*/true);

  if (reload()) {
    RecreateAndEnableFatalCrashEventsObserver(fatal_crash_events_observer,
                                              &result_metric_data);
  }

  // Create kMaxNumOfLocalIds - 1 crashes with an earlier capture time.
  for (size_t i = 0u; i < kMaxNumOfLocalIds - 1u; ++i) {
    std::ostringstream ss;
    ss << kLocalIdEarly << i;
    CreateFatalCrashEvent(/*local_id=*/ss.str(),
                          /*capture_time=*/kCaptureTimeEarly,
                          *fatal_crash_events_observer, &result_metric_data,
                          /*is_uploaded=*/false);
  }

  // Because the first crash (which has a later capture time) has been uploaded,
  // it should no longer be saved. Therefore, the kMaxNumOfLocalIds'th early
  // crash should be saved.
  auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
  crash_event_info->local_id = kLocalIdEarly;
  crash_event_info->capture_time = kCaptureTimeEarly;
  const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
      std::move(crash_event_info), fatal_crash_events_observer.get(),
      &result_metric_data);
  ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
  EXPECT_EQ(fatal_crash_telemetry.local_id(), kLocalIdEarly);
  ASSERT_TRUE(fatal_crash_telemetry.has_timestamp_us());
  EXPECT_EQ(
      fatal_crash_telemetry.timestamp_us(),
      FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeEarly));
}

// Tests that if the thread is interrupted somehow when the event is being
// processed (e.g., ash crashes), a crash with the same local ID should still be
// reported.
TEST_F(FatalCrashEventsObserverReportedLocalIdsTest,
       RepeatedLocalIDReportedIfFirstTimeIsInterrupted) {
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  {
    // Simulate the thread is interrupted after event observed callback is
    // called.
    ScopedInterruptedAfterEventObserved scoped_interruption(
        *fatal_crash_events_observer);
    CreateFatalCrashEvent(
        /*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
        *fatal_crash_events_observer, &result_metric_data);
  }

  // Always reload to simulate what happens practically, e.g., ash crashes.
  fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

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

TEST_F(FatalCrashEventsObserverReportedLocalIdsTest,
       CrashReportedAsUploadedIsNotRemovedIfInterrupted) {
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  // Same crash, first reported as unuploaded, then reported again as uploaded.
  CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                        *fatal_crash_events_observer, &result_metric_data,
                        /*is_uploaded=*/false);

  {
    // Simulate the thread is interrupted after event observed callback is
    // called.
    ScopedInterruptedAfterEventObserved scoped_interruption(
        *fatal_crash_events_observer);
    CreateFatalCrashEvent(/*local_id=*/kLocalId, /*capture_time=*/kCaptureTime,
                          *fatal_crash_events_observer, &result_metric_data,
                          /*is_uploaded=*/true);
  }

  // Reload.
  RecreateAndEnableFatalCrashEventsObserver(fatal_crash_events_observer,
                                            &result_metric_data);

  // Create kMaxNumOfLocalIds - 1 crashes with an earlier capture time.
  for (size_t i = 0u; i < kMaxNumOfLocalIds - 1u; ++i) {
    std::ostringstream ss;
    ss << kLocalIdEarly << i;
    CreateFatalCrashEvent(/*local_id=*/ss.str(),
                          /*capture_time=*/kCaptureTimeEarly,
                          *fatal_crash_events_observer, &result_metric_data,
                          /*is_uploaded=*/false);
  }

  // Because the first crash (which has a later capture time) has been uploaded,
  // it should no longer be saved. However, because the thread is interrupted,
  // it remains in the saved local IDs. Therefore, the kMaxNumOfLocalIds'th
  // early crash would not be saved here.
  auto local_id_entry = WaitForSkippedFatalCrashEvent(
      /*local_id=*/kLocalIdEarly,
      /*capture_time=*/kCaptureTimeEarly, *fatal_crash_events_observer);
  EXPECT_EQ(local_id_entry.local_id, kLocalIdEarly);
  EXPECT_EQ(
      local_id_entry.capture_timestamp_us,
      FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeEarly));
  local_id_entry = WaitForSkippedFatalCrashEvent(
      /*local_id=*/kLocalIdEarly,
      /*capture_time=*/kCaptureTimeEarly, *fatal_crash_events_observer);
  EXPECT_EQ(local_id_entry.local_id, kLocalIdEarly);
  EXPECT_EQ(
      local_id_entry.capture_timestamp_us,
      FatalCrashEventsObserver::ConvertTimeToMicroseconds(kCaptureTimeEarly));
}

TEST_F(FatalCrashEventsObserverReportedLocalIdsTest,
       ReconstructLocalIdEntryQueueAfterMaxIsReached) {
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  // Report an unuploaded crash with an early capture time.
  CreateFatalCrashEvent(/*local_id=*/kLocalIdEarly,
                        /*capture_time=*/kCaptureTimeEarly,
                        *fatal_crash_events_observer, &result_metric_data,
                        /*is_uploaded=*/false);

  // Create kMaxSizeOfLocalIdEntryQueue - 1 crashes with a later capture time.
  // Report them as unuploaded first and then uploaded. This should be able to
  // fill the priority queue up with the local IDs of uploaded crashes.
  for (size_t i = 0u; i < kMaxSizeOfLocalIdEntryQueue - 1u; ++i) {
    std::ostringstream ss;
    ss << kLocalIdLate << i;
    CreateFatalCrashEvent(/*local_id=*/ss.str(),
                          /*capture_time=*/kCaptureTimeLate,
                          *fatal_crash_events_observer, &result_metric_data,
                          /*is_uploaded=*/false);
    CreateFatalCrashEvent(/*local_id=*/ss.str(),
                          /*capture_time=*/kCaptureTimeLate,
                          *fatal_crash_events_observer, &result_metric_data,
                          /*is_uploaded=*/true);
  }

  // Sanity check: The priority queue is large.
  ASSERT_EQ(fatal_crash_test_environment_.GetLocalIdEntryQueueSize(
                *fatal_crash_events_observer),
            kMaxSizeOfLocalIdEntryQueue);

  // One more unuploaded crash and the queue should be constructed.
  CreateFatalCrashEvent(/*local_id=*/kLocalIdLate,
                        /*capture_time=*/kCaptureTimeLate,
                        *fatal_crash_events_observer, &result_metric_data,
                        /*is_uploaded=*/false);
  EXPECT_EQ(fatal_crash_test_environment_.GetLocalIdEntryQueueSize(
                *fatal_crash_events_observer),
            2u);
}

TEST_F(FatalCrashEventsObserverReportedLocalIdsTest,
       SlowFileWritingSaveFileWritten) {
  static constexpr uint64_t kNumOfEvents = 3u;
  const auto kMaxLocalId = base::NumberToString(kNumOfEvents - 1);

  const auto io_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  // Create and set up the observer object.
  auto observer = fatal_crash_test_environment_.CreateFatalCrashEventsObserver(
      /*reported_local_id_io_task_runner=*/io_task_runner,
      /*uploaded_crash_info_io_task_runner=*/nullptr);
  observer->SetReportingEnabled(true);

  // Make sure file loading IO has finished.
  FatalCrashEventsObserver::TestEnvironment::FlushIoTasks(*observer);

  // Block the IO thread to simulate slow file writing.
  FatalCrashEventsObserver::TestEnvironment::SequenceBlocker sequence_blocker(
      io_task_runner);

  // Create a few events.
  for (uint64_t i = 0u; i < kNumOfEvents; ++i) {
    auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
    crash_event_info->local_id = base::NumberToString(i);

    FakeCrosHealthd::Get()->EmitEventForCategory(
        EventCategoryEnum::kCrash,
        EventInfo::NewCrashEventInfo(std::move(crash_event_info)));
  }

  // Flush current thread so that all file writing IO tasks are posted.
  base::RunLoop().RunUntilIdle();

  // Release the IO thread and flush IO (done when recreating the fatal crash
  // events observer).
  sequence_blocker.Unblock();
  RecreateAndEnableFatalCrashEventsObserver(observer);

  // Events with duplicate local IDs are skipped, because the save file is
  // correctly written.
  EXPECT_THAT(
      WaitForSkippedFatalCrashEvent(kMaxLocalId, kCaptureTime, *observer),
      AllOf(Field(&FatalCrashEventsObserver::LocalIdEntry::local_id,
                  StrEq(kMaxLocalId)),
            Field(&FatalCrashEventsObserver::LocalIdEntry::capture_timestamp_us,
                  Eq(FatalCrashEventsObserver::ConvertTimeToMicroseconds(
                      kCaptureTime)))));
}

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

  // (kMaxNumOfLocalIds - 1) more crashes to fill saved local IDs to maximum
  // capacity.
  for (size_t i = 0u; i < kMaxNumOfLocalIds - 1; ++i) {
    std::ostringstream ss;
    ss << kLocalId << i;
    CreateFatalCrashEvent(/*local_id=*/ss.str(),
                          /*capture_time=*/kCaptureTimeZero,
                          *fatal_crash_events_observer, &result_metric_data);
  }

  // Because one good line is still parsed and loaded, the
  // (kMaxNumOfLocalIds+1)'th crash with the zero capture time would be skipped.
  const auto local_id_entry = WaitForSkippedFatalCrashEvent(
      /*local_id=*/kLocalId,
      /*capture_time=*/kCaptureTimeZero, *fatal_crash_events_observer);
}

TEST_F(FatalCrashEventsObserverReportedLocalIdsCorruptSaveFileTest,
       UnreadableSaveFileIgnored) {
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);
  CreateFatalCrashEvent(kLocalId, kCaptureTime, *fatal_crash_events_observer,
                        &result_metric_data);

  // Make sure the save file writing task is executed.
  FatalCrashEventsObserver::TestEnvironment::FlushIoTasks(
      *fatal_crash_events_observer);

  // The save file is now available. Make it unreadable.
  ASSERT_TRUE(base::PathExists(GetSaveFilePath()));
  ASSERT_TRUE(base::MakeFileUnreadable(GetSaveFilePath()));

  // Reload to force loading from the save file.
  fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  // Verify that no crash is loaded by receiving an unuploaded crash with the
  // same local ID.
  auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/false);
  crash_event_info->local_id = kLocalId;
  const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
      std::move(crash_event_info), fatal_crash_events_observer.get(),
      &result_metric_data);
  ASSERT_TRUE(fatal_crash_telemetry.has_local_id());
  EXPECT_EQ(fatal_crash_telemetry.local_id(), kLocalId);
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

// Tests `FatalCrashEventsObserver` with uploaded crashes with a focus on
// saved states.
class FatalCrashEventsObserverUploadedCrashTestBase
    : public FatalCrashEventsObserverTestBase {
 public:
  static constexpr std::string_view kCreationTimestampMsJsonKey{
      FatalCrashEventsObserver::TestEnvironment::kCreationTimestampMsJsonKey};
  static constexpr std::string_view kOffsetJsonKey{
      FatalCrashEventsObserver::TestEnvironment::kOffsetJsonKey};

  FatalCrashEventsObserverUploadedCrashTestBase(
      const FatalCrashEventsObserverUploadedCrashTestBase&) = delete;
  FatalCrashEventsObserverUploadedCrashTestBase& operator=(
      const FatalCrashEventsObserverUploadedCrashTestBase&) = delete;

 protected:
  // Fields used for tests.
  static constexpr std::string_view kCrashReportId = "crash report ID";

  FatalCrashEventsObserverUploadedCrashTestBase() = default;
  ~FatalCrashEventsObserverUploadedCrashTestBase() override = default;

  // Gets the path to the save file.
  base::FilePath GetSaveFilePath() const {
    return fatal_crash_test_environment_.GetUploadedCrashInfoSaveFilePath();
  }

  // Generates an uninteresting fatal crash event to alter the observer's state
  // in preparation for the test.
  void CreateFatalCrashEvent(
      std::string_view crash_report_id,
      base::Time creation_time,
      uint64_t offset,
      FatalCrashEventsObserver& fatal_crash_observer,
      base::test::TestFuture<MetricData>* test_event = nullptr) {
    auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/true);
    crash_event_info->upload_info->crash_report_id = crash_report_id;
    crash_event_info->upload_info->creation_time = creation_time;
    crash_event_info->upload_info->offset = offset;

    const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
        std::move(crash_event_info), &fatal_crash_observer, test_event);
    ASSERT_TRUE(fatal_crash_telemetry.has_crash_report_id());
    ASSERT_EQ(fatal_crash_telemetry.crash_report_id(), crash_report_id);
  }

  // Wait for the given fatal crash event being skipped.
  std::tuple<std::string /* crash_report_id */,
             base::Time /* creation_time */,
             uint64_t /* offset */>
  WaitForSkippedFatalCrashEvent(
      std::string_view crash_report_id,
      base::Time creation_time,
      uint64_t offset,
      FatalCrashEventsObserver& fatal_crash_observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(
        FatalCrashEventsObserver::TestEnvironment::GetTestSettings(
            fatal_crash_observer)
            .sequence_checker);

    base::test::TestFuture<std::string /* crash_report_id */,
                           base::Time /* creation_time */,
                           uint64_t /* offset */>
        result;
    FatalCrashEventsObserver::TestEnvironment::GetTestSettings(
        fatal_crash_observer)
        .skipped_uploaded_crash_callback = result.GetRepeatingCallback();

    auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/true);
    crash_event_info->upload_info->crash_report_id = crash_report_id;
    crash_event_info->upload_info->creation_time = creation_time;
    crash_event_info->upload_info->offset = offset;
    FakeCrosHealthd::Get()->EmitEventForCategory(
        EventCategoryEnum::kCrash,
        EventInfo::NewCrashEventInfo(std::move(crash_event_info)));

    return result.Take();
  }
};

struct FatalCrashEventsObserverUploadedCrashCase {
  base::Time creation_time;
  uint64_t offset;
  bool should_be_reported;
  bool creatable_save_file = true;
  bool interrupt_after_event_observed = false;
};

// Tests whether uploads.log creation time and offset are respected in deciding
// whether an uploaded crash should be reported.
class FatalCrashEventsObserverUploadedCrashTest
    : public FatalCrashEventsObserverUploadedCrashTestBase,
      public ::testing::WithParamInterface<
          std::tuple<FatalCrashEventsObserverUploadedCrashCase,
                     /*reload=*/bool>> {
 public:
  static constexpr base::Time kCreationTime = base::Time::FromTimeT(10u);
  static constexpr base::Time kEarlierCreationTime = base::Time::FromTimeT(1u);
  static constexpr base::Time kLaterCreationTime = base::Time::FromTimeT(100u);
  static constexpr uint64_t kOffset = 20u;
  static constexpr uint64_t kSmallerOffset = 2u;
  static constexpr uint64_t kLargerOffset = 200u;

  FatalCrashEventsObserverUploadedCrashTest(
      const FatalCrashEventsObserverUploadedCrashTest&) = delete;
  FatalCrashEventsObserverUploadedCrashTest& operator=(
      const FatalCrashEventsObserverUploadedCrashTest&) = delete;

  // Gets the name as used in the test name given a creation time.
  static constexpr std::string_view GetTestNameForCreationTime(
      base::Time creation_time) {
    if (creation_time >
        FatalCrashEventsObserverUploadedCrashTest::kCreationTime) {
      return "later_time";
    } else if (creation_time <
               FatalCrashEventsObserverUploadedCrashTest::kCreationTime) {
      return "earlier_time";
    } else {
      return "same_time";
    }
  }

  // Gets the name as used in the test name given a offset.
  static constexpr std::string_view GetTestNameForOffset(uint64_t offset) {
    if (offset > FatalCrashEventsObserverUploadedCrashTest::kOffset) {
      return "larger_offset";
    } else if (offset < FatalCrashEventsObserverUploadedCrashTest::kOffset) {
      return "smaller_offset";
    } else {
      return "same_offset";
    }
  }

 protected:
  FatalCrashEventsObserverUploadedCrashTest() = default;
  ~FatalCrashEventsObserverUploadedCrashTest() override = default;

  // Whether the fatal crash events observer should reload to simulate user
  // restarting ash.
  bool reload() const { return std::get<1>(GetParam()); }

  // Creation time of the second uploaded crash.
  base::Time creation_time() const {
    return std::get<0>(GetParam()).creation_time;
  }

  // Offset of the second uploaded crash.
  uint64_t offset() const { return std::get<0>(GetParam()).offset; }

  // Should the second uploaded crash be reported?
  bool should_be_reported() const {
    return std::get<0>(GetParam()).should_be_reported;
  }

  // Should the save file be made creatable.
  bool creatable_save_file() const {
    return std::get<0>(GetParam()).creatable_save_file;
  }

  // Should the first event be interrupted after the event observed callback.
  // Used to simulate the thread is interrupted somehow when the event is being
  // processed (e.g., ash crashes).
  bool interrupt_after_event_observed() const {
    return std::get<0>(GetParam()).interrupt_after_event_observed;
  }
};

TEST_P(FatalCrashEventsObserverUploadedCrashTest,
       ReportBasedOnCreationTimeAndOffset) {
  // Report an uploaded crash with kCreationTime and kOffset. Then, receiving a
  // second uploaded crash with the given creation time and offset in the
  // parameters. Tests the second crash is properly reported or skipped.

  static constexpr std::string_view kAnotherCrashReportId =
      "Another Crash Report ID";

  if (!creatable_save_file()) {
    ASSERT_TRUE(base::MakeFileUnwritable(GetSaveFilePath().DirName()));
  }

  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  {
    // Simulate the thread is interrupted after event observed callback is
    // called, if required by the test params.
    auto scoped_interruption =
        interrupt_after_event_observed()
            ? std::make_unique<ScopedInterruptedAfterEventObserved>(
                  *fatal_crash_events_observer)
            : nullptr;

    CreateFatalCrashEvent(kCrashReportId, kCreationTime, kOffset,
                          *fatal_crash_events_observer, &result_metric_data);
  }

  if (reload()) {
    RecreateAndEnableFatalCrashEventsObserver(fatal_crash_events_observer,
                                              &result_metric_data);
  }

  if (should_be_reported()) {
    // If the uploaded crash should be reported, we test that it is indeed
    // reported by comparing the crash report ID.
    auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/true);
    crash_event_info->upload_info->crash_report_id = kAnotherCrashReportId;
    crash_event_info->upload_info->creation_time = creation_time();
    crash_event_info->upload_info->offset = offset();
    const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
        std::move(crash_event_info), fatal_crash_events_observer.get(),
        &result_metric_data);
    ASSERT_TRUE(fatal_crash_telemetry.has_crash_report_id());
    EXPECT_EQ(fatal_crash_telemetry.crash_report_id(), kAnotherCrashReportId);
  } else {
    // If the uploaded crash should not be reported, we expect a skipped event.
    const auto [skipped_crash_report_id, skipped_creation_time,
                skipped_offset] =
        WaitForSkippedFatalCrashEvent(kAnotherCrashReportId, creation_time(),
                                      offset(), *fatal_crash_events_observer);
    EXPECT_EQ(skipped_crash_report_id, kAnotherCrashReportId);
    EXPECT_EQ(skipped_creation_time, creation_time());
    EXPECT_EQ(skipped_offset, offset());
  }
}

TEST_F(FatalCrashEventsObserverUploadedCrashTest,
       SlowFileWritingSaveFileWritten) {
  static constexpr uint64_t kNumOfEvents = 3u;
  static constexpr auto kCreationTime = base::Time::UnixEpoch();

  const auto io_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});

  // Create and set up the observer object.
  auto observer = fatal_crash_test_environment_.CreateFatalCrashEventsObserver(
      /*reported_local_id_io_task_runner=*/nullptr,
      /*uploaded_crash_info_io_task_runner=*/io_task_runner);
  observer->SetReportingEnabled(true);

  // Make sure file loading IO has finished.
  FatalCrashEventsObserver::TestEnvironment::FlushIoTasks(*observer);

  // Block the IO thread to simulate slow file writing.
  FatalCrashEventsObserver::TestEnvironment::SequenceBlocker sequence_blocker(
      io_task_runner);

  // Create a few events.
  for (uint64_t i = 0u; i < kNumOfEvents; ++i) {
    auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/true);
    crash_event_info->local_id = base::NumberToString(i);
    // Incremental offset, otherwise the later uploaded crashes would not be
    // reported.
    crash_event_info->upload_info->offset = i;
    crash_event_info->upload_info->creation_time = kCreationTime;

    FakeCrosHealthd::Get()->EmitEventForCategory(
        EventCategoryEnum::kCrash,
        EventInfo::NewCrashEventInfo(std::move(crash_event_info)));
  }

  // Flush current thread so that all file writing IO tasks are posted.
  base::RunLoop().RunUntilIdle();

  // Release the IO thread and flush IO (done when recreating the fatal crash
  // events observer).
  sequence_blocker.Unblock();
  RecreateAndEnableFatalCrashEventsObserver(observer);

  // Events with low offset are skipped, because the save file is correctly
  // written.
  EXPECT_THAT(
      WaitForSkippedFatalCrashEvent(kCrashReportId, kCreationTime,
                                    /*offset=*/kNumOfEvents - 1, *observer),
      FieldsAre(StrEq(kCrashReportId), Eq(kCreationTime),
                Eq(kNumOfEvents - 1)));
}

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverUploadedCrashTestsForCorrectlyComparingCreationTimeAndOffset,
    FatalCrashEventsObserverUploadedCrashTest,
    ::testing::Combine(
        ::testing::ValuesIn(std::vector<
                            FatalCrashEventsObserverUploadedCrashCase>{
            {.creation_time = FatalCrashEventsObserverUploadedCrashTest::
                 kEarlierCreationTime,
             .offset =
                 FatalCrashEventsObserverUploadedCrashTest::kSmallerOffset,
             .should_be_reported = false},
            {.creation_time = FatalCrashEventsObserverUploadedCrashTest::
                 kEarlierCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kOffset,
             .should_be_reported = false},
            {.creation_time = FatalCrashEventsObserverUploadedCrashTest::
                 kEarlierCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kLargerOffset,
             .should_be_reported = false},
            {.creation_time =
                 FatalCrashEventsObserverUploadedCrashTest::kCreationTime,
             .offset =
                 FatalCrashEventsObserverUploadedCrashTest::kSmallerOffset,
             .should_be_reported = false},
            {.creation_time =
                 FatalCrashEventsObserverUploadedCrashTest::kCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kOffset,
             .should_be_reported = false},
            {.creation_time =
                 FatalCrashEventsObserverUploadedCrashTest::kCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kLargerOffset,
             .should_be_reported = true},
            {.creation_time =
                 FatalCrashEventsObserverUploadedCrashTest::kLaterCreationTime,
             .offset =
                 FatalCrashEventsObserverUploadedCrashTest::kSmallerOffset,
             .should_be_reported = true},
            {.creation_time =
                 FatalCrashEventsObserverUploadedCrashTest::kLaterCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kOffset,
             .should_be_reported = true},
            {.creation_time =
                 FatalCrashEventsObserverUploadedCrashTest::kLaterCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kLargerOffset,
             .should_be_reported = true},
        }),
        /*reload=*/::testing::Bool()),
    [](const testing::TestParamInfo<
        FatalCrashEventsObserverUploadedCrashTest::ParamType>& info) {
      return base::StrCat(
          {FatalCrashEventsObserverUploadedCrashTest::
               GetTestNameForCreationTime(
                   std::get<0>(info.param).creation_time),
           "_",
           FatalCrashEventsObserverUploadedCrashTest::GetTestNameForOffset(
               std::get<0>(info.param).offset),
           "_", std::get<1>(info.param) ? "reload" : "same_session"});
    });

// Even if the save file can't be created, the unreloaded result should be the
// same. No need to test it for all combinations of creation times and offsets;
// only pick 2 cases, one that should report and another one that should not be
// reported.
INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverUploadedCrashTestsForUncreatableSaveFile,
    FatalCrashEventsObserverUploadedCrashTest,
    ::testing::Combine(
        ::testing::ValuesIn(std::vector<
                            FatalCrashEventsObserverUploadedCrashCase>{
            {.creation_time = FatalCrashEventsObserverUploadedCrashTest::
                 kEarlierCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kOffset,
             .should_be_reported = false,
             .creatable_save_file = false},
            {.creation_time =
                 FatalCrashEventsObserverUploadedCrashTest::kLaterCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kOffset,
             .should_be_reported = true,
             .creatable_save_file = false},
        }),
        /*reload=*/::testing::Values(false)),
    [](const testing::TestParamInfo<
        FatalCrashEventsObserverUploadedCrashTest::ParamType>& info) {
      return base::StrCat(
          {FatalCrashEventsObserverUploadedCrashTest::
               GetTestNameForCreationTime(
                   std::get<0>(info.param).creation_time),
           "_",
           FatalCrashEventsObserverUploadedCrashTest::GetTestNameForOffset(
               std::get<0>(info.param).offset),
           "_uncreatable_file"});
    });

// Tests that if the thread is interrupted right after the on event callback
// (e.g., ash crashes), the event that has been processed would not affect the
// state of the observer and a later crash with the same creation time and
// offset should still be reported.
INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverUploadedCrashTestsForInterruptionAfterOnEvent,
    FatalCrashEventsObserverUploadedCrashTest,
    ::testing::Values(
        std::make_tuple<FatalCrashEventsObserverUploadedCrashCase, bool>(
            {.creation_time =
                 FatalCrashEventsObserverUploadedCrashTest::kCreationTime,
             .offset = FatalCrashEventsObserverUploadedCrashTest::kOffset,
             .should_be_reported = true,
             .interrupt_after_event_observed = true},
            /*reload=*/true)),
    [](const testing::TestParamInfo<
        FatalCrashEventsObserverUploadedCrashTest::ParamType>& info) {
      return base::StrCat(
          {FatalCrashEventsObserverUploadedCrashTest::
               GetTestNameForCreationTime(
                   std::get<0>(info.param).creation_time),
           "_",
           FatalCrashEventsObserverUploadedCrashTest::GetTestNameForOffset(
               std::get<0>(info.param).offset),
           "_interrupted"});
    });

struct FatalCrashEventsObserverUploadedCrashCorruptSaveFileCase {
  std::string name;
  std::string save_file_content;
};

// Tests uploaded crash but the save file is corrupted.
class FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest
    : public FatalCrashEventsObserverUploadedCrashTestBase,
      public ::testing::WithParamInterface<
          FatalCrashEventsObserverUploadedCrashCorruptSaveFileCase> {
 public:
  FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest(
      const FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest&) = delete;
  FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest& operator=(
      const FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest&) = delete;

 protected:
  FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest() = default;
  ~FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest() override =
      default;

  const std::string& save_file_content() const {
    return GetParam().save_file_content;
  }
};

TEST_P(FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest,
       CorruptFileIgnored) {
  ASSERT_TRUE(base::WriteFile(GetSaveFilePath(), save_file_content()));
  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  // Verify that no crash is loaded by receiving an uploaded crash with zero
  // uploads.log creation time and offset.
  auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/true);
  crash_event_info->upload_info->creation_time = base::Time::FromTimeT(0u);
  crash_event_info->upload_info->offset = 0u;
  crash_event_info->upload_info->crash_report_id = kCrashReportId;
  const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
      std::move(crash_event_info), fatal_crash_events_observer.get(),
      &result_metric_data);
  ASSERT_TRUE(fatal_crash_telemetry.has_crash_report_id());
  EXPECT_EQ(fatal_crash_telemetry.crash_report_id(), kCrashReportId);
}

TEST_F(FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest,
       UnreadableSaveFileIgnored) {
  static constexpr base::Time kZeroCreationTime = base::Time::FromTimeT(0u);
  static constexpr uint64_t kZeroOffset = 0u;

  base::test::TestFuture<MetricData> result_metric_data;
  auto fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);
  CreateFatalCrashEvent(kCrashReportId, kZeroCreationTime, kZeroOffset,
                        *fatal_crash_events_observer, &result_metric_data);

  // Make sure the save file writing task is executed.
  FatalCrashEventsObserver::TestEnvironment::FlushIoTasks(
      *fatal_crash_events_observer);

  // The save file is now available. Make it unreadable.
  ASSERT_TRUE(base::PathExists(GetSaveFilePath()));
  ASSERT_TRUE(base::MakeFileUnreadable(GetSaveFilePath()));

  // Reload to force loading from the save file.
  fatal_crash_events_observer =
      CreateAndEnableFatalCrashEventsObserver(&result_metric_data);

  // Verify that no crash is loaded by receiving an uploaded crash with zero
  // uploads.log creation time and offset.
  auto crash_event_info = NewCrashEventInfo(/*is_uploaded=*/true);
  crash_event_info->upload_info->creation_time = kZeroCreationTime;
  crash_event_info->upload_info->offset = kZeroOffset;
  crash_event_info->upload_info->crash_report_id = kCrashReportId;
  const auto fatal_crash_telemetry = WaitForFatalCrashTelemetry(
      std::move(crash_event_info), fatal_crash_events_observer.get(),
      &result_metric_data);
  ASSERT_TRUE(fatal_crash_telemetry.has_crash_report_id());
  EXPECT_EQ(fatal_crash_telemetry.crash_report_id(), kCrashReportId);
}

INSTANTIATE_TEST_SUITE_P(
    FatalCrashEventsObserverUploadedCrashCorruptSaveFileTests,
    FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest,
    ::testing::ValuesIn(
        std::vector<FatalCrashEventsObserverUploadedCrashCorruptSaveFileCase>{
            {.name = "empty", .save_file_content = ""},
            {.name = "not_json", .save_file_content = "some_string"},
            {.name = "not_valid_json",
             .save_file_content = "{missing_a_brace = True"},
            {.name = "not_a_dict_json",
             .save_file_content = "[\"i_am_a_list\"]"},
            {.name = "no_creation_timestamp_ms_key",
             .save_file_content = base::StrCat(
                 {"{\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::kOffsetJsonKey,
                  "\" = 10}"})},
            {.name = "no_offset_key",
             .save_file_content =
                 base::StrCat({"{\"",
                               FatalCrashEventsObserverUploadedCrashTestBase::
                                   kCreationTimestampMsJsonKey,
                               "\" = 1}"})},
            {.name = "timestamp_not_a_number",
             .save_file_content = base::StrCat(
                 {"{\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::
                      kCreationTimestampMsJsonKey,
                  "\" = \"not a number\",\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::kOffsetJsonKey,
                  "\" = 10}"})},
            {.name = "negative_timestamp",
             .save_file_content = base::StrCat(
                 {"{\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::
                      kCreationTimestampMsJsonKey,
                  "\" = -1,\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::kOffsetJsonKey,
                  "\" = 10}"})},
            {.name = "offset_not_a_number",
             .save_file_content = base::StrCat(
                 {"{\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::
                      kCreationTimestampMsJsonKey,
                  "\" = 1,\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::kOffsetJsonKey,
                  "\" = \"not a number\"}"})},
            {.name = "negative_offset",
             .save_file_content = base::StrCat(
                 {"{\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::
                      kCreationTimestampMsJsonKey,
                  "\" = 1,\"",
                  FatalCrashEventsObserverUploadedCrashTestBase::kOffsetJsonKey,
                  "\" = -10}"})},
        }),
    [](const testing::TestParamInfo<
        FatalCrashEventsObserverUploadedCrashCorruptSaveFileTest::ParamType>&
           info) { return info.param.name; });
}  // namespace reporting
