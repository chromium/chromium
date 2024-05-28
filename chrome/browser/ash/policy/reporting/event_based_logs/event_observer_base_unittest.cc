// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observer_base.h"

#include <set>

#include "base/json/values_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/policy_pref_names.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Hours;

namespace {

// A fake implementation of `EventObserverBase` for testing.
class TestEventObserver : public policy::EventObserverBase {
 public:
  ash::reporting::TriggerEventType GetEventType() const override {
    return ash::reporting::TriggerEventType::TRIGGER_EVENT_TYPE_UNSPECIFIED;
  }

  std::set<support_tool::DataCollectorType> GetDataCollectorTypes()
      const override {
    return {support_tool::DataCollectorType::CHROME_INTERNAL,
            support_tool::DataCollectorType::CHROMEOS_NETWORK_HEALTH};
  }
};

class EventObserverBaseTest : public testing::Test {
 public:
  EventObserverBaseTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing_local_state_.Get()->registry()->RegisterDictionaryPref(
        policy::prefs::kEventBasedLogLastUploadTimes);
  }

  void SetLastUploadTime(const std::string event_name,
                         base::Time last_upload_time) {
    testing_local_state_.Get()->SetDict(
        policy::prefs::kEventBasedLogLastUploadTimes,
        base::Value::Dict().Set(event_name,
                                base::TimeToValue(last_upload_time)));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState testing_local_state_;
};

}  // namespace

TEST_F(EventObserverBaseTest, SuccessfulFirstUpload) {
  TestEventObserver event_observer;
  base::test::TestFuture<policy::EventBasedUploadStatus> test_future;
  event_observer.TriggerLogUpload(
      policy::EventBasedLogUploader::GenerateUploadId(),
      test_future.GetCallback());
  ASSERT_EQ(test_future.Take(), policy::EventBasedUploadStatus::kSuccess);
}

TEST_F(EventObserverBaseTest, SuccessfulUploadAfterTimeLimit) {
  TestEventObserver event_observer;

  // Set last upload time as more than the default time limit (24 hours).
  SetLastUploadTime(event_observer.GetEventName(),
                    base::Time::NowFromSystemTime() - Hours(25));

  base::test::TestFuture<policy::EventBasedUploadStatus> test_future;
  event_observer.TriggerLogUpload(
      policy::EventBasedLogUploader::GenerateUploadId(),
      test_future.GetCallback());
  ASSERT_EQ(test_future.Take(), policy::EventBasedUploadStatus::kSuccess);
}

TEST_F(EventObserverBaseTest, DeclinedUploadBeforeTimeLimit) {
  TestEventObserver event_observer;

  // Set last upload time as less than the default time limit (24 hours).
  SetLastUploadTime(event_observer.GetEventName(),
                    base::Time::NowFromSystemTime() - Hours(22));

  base::test::TestFuture<policy::EventBasedUploadStatus> test_future;
  event_observer.TriggerLogUpload(
      policy::EventBasedLogUploader::GenerateUploadId(),
      test_future.GetCallback());
  ASSERT_EQ(test_future.Take(), policy::EventBasedUploadStatus::kDeclined);
}

TEST_F(EventObserverBaseTest, DeclinedUploadForDifferentEventType) {
  TestEventObserver event_observer;

  // Set last upload time for a different event type as less than the default
  // time limit (24 hours).
  SetLastUploadTime("TRIGGER_EVENT_TYPE_OTHER",
                    base::Time::NowFromSystemTime() - Hours(22));

  base::test::TestFuture<policy::EventBasedUploadStatus> test_future;
  event_observer.TriggerLogUpload(
      policy::EventBasedLogUploader::GenerateUploadId(),
      test_future.GetCallback());
  ASSERT_EQ(test_future.Take(), policy::EventBasedUploadStatus::kSuccess);
}
