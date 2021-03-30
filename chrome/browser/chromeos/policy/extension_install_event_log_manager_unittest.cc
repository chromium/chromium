// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_manager.h"

#include <iterator>
#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/extension_install_event_log.h"
#include "chrome/browser/chromeos/policy/install_event_log_util.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/util/status.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/quota_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Invoke;
using testing::Matcher;
using testing::Mock;
using testing::Pointee;
using testing::Return;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr base::FilePath::CharType kLogFileName[] =
    FILE_PATH_LITERAL("extension_install_log");
constexpr base::TimeDelta kStoreDelay = base::TimeDelta::FromSeconds(5);
constexpr base::TimeDelta kUploadInterval = base::TimeDelta::FromHours(3);
constexpr base::TimeDelta kExpeditedUploadDelay =
    base::TimeDelta::FromMinutes(15);
constexpr base::TimeDelta kOneMs = base::TimeDelta::FromMilliseconds(1);

constexpr int kTotalSizeExpeditedUploadThreshold = 2048;
constexpr int kMaxSizeExpeditedUploadThreshold = 512;

constexpr const char* kExtensionIds[] = {
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "cccccccccccccccccccccccccccccccc", "dddddddddddddddddddddddddddddddd",
    "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"};

using Events = std::map<extensions::ExtensionId,
                        std::vector<em::ExtensionInstallReportLogEvent>>;

bool ContainsSameEvents(const Events& expected,
                        const em::ExtensionInstallReportRequest& actual) {
  if (actual.extension_install_reports_size() !=
      static_cast<int>(expected.size())) {
    return false;
  }
  for (const auto& expected_extension_log : expected) {
    bool extension_found = false;
    for (int i = 0; i < actual.extension_install_reports_size(); ++i) {
      const auto& actual_extension_log = actual.extension_install_reports(i);
      if (actual_extension_log.extension_id() == expected_extension_log.first) {
        if (actual_extension_log.logs_size() !=
            static_cast<int>(expected_extension_log.second.size())) {
          return false;
        }
        for (int j = 0;
             j < static_cast<int>(expected_extension_log.second.size()); ++j) {
          if (actual_extension_log.logs(j).SerializePartialAsString() !=
              expected_extension_log.second[j].SerializePartialAsString()) {
            return false;
          }
        }
        extension_found = true;
        break;
      }
    }
    if (!extension_found) {
      return false;
    }
  }
  return true;
}

base::Value ConvertEventsToValue(const Events& events, Profile* profile) {
  base::Value context = reporting::GetContext(profile);
  base::Value event_list(base::Value::Type::LIST);

  for (auto it = events.begin(); it != events.end(); ++it) {
    const extensions::ExtensionId& extension_id = (*it).first;
    for (const em::ExtensionInstallReportLogEvent&
             extension_install_report_log_event : (*it).second) {
      base::Value wrapper;
      wrapper = ConvertExtensionEventToValue(
          extension_id, extension_install_report_log_event, context);
      event_list.Append(std::move(wrapper));
    }
  }

  return event_list;
}

MATCHER_P(MatchEvents, expected, "contains events") {
  DCHECK(expected);
  std::string expected_serialized_string;
  JSONStringValueSerializer expected_serializer(&expected_serialized_string);
  if (!expected_serializer.Serialize(*expected))
    return false;

  return arg == expected_serialized_string;
}

class TestLogTaskRunnerWrapper
    : public ExtensionInstallEventLogManager::LogTaskRunnerWrapper {
 public:
  TestLogTaskRunnerWrapper() {
    test_task_runner_ = new base::TestSimpleTaskRunner;
  }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override {
    return test_task_runner_;
  }

  base::TestSimpleTaskRunner* test_task_runner() const {
    return test_task_runner_.get();
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_;
};

}  // namespace

class ExtensionInstallEventLogManagerTest : public testing::Test {
 protected:
  ExtensionInstallEventLogManagerTest()
      : uploader_(/*profile=*/nullptr),
        log_task_runner_(log_task_runner_wrapper_.test_task_runner()),
        log_file_path_(profile_.GetPath().Append(kLogFileName)),
        extension_ids_{std::begin(kExtensionIds), std::end(kExtensionIds)},
        events_value_(base::Value::Type::DICTIONARY),
        scoped_fake_statistics_provider_(
            std::make_unique<
                chromeos::system::ScopedFakeStatisticsProvider>()) {}

  // testing::Test:
  void SetUp() override {
    auto mock_report_queue = std::make_unique<reporting::MockReportQueue>();
    mock_report_queue_ = mock_report_queue.get();
    uploader_.SetReportQueue(std::move(mock_report_queue));
    event_.set_timestamp(0);
    event_.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);

    scoped_main_task_runner_ =
        std::make_unique<base::ScopedMockTimeMessageLoopTaskRunner>();
    main_task_runner_ = scoped_main_task_runner_->task_runner();
  }

  // testing::Test:
  void TearDown() override {
    Mock::VerifyAndClearExpectations(mock_report_queue_);
    manager_.reset();
    FastForwardUntilNoTasksRemain();

    main_task_runner_ = nullptr;
    scoped_main_task_runner_.reset();
  }

  void CreateManager() {
    manager_ = std::make_unique<ExtensionInstallEventLogManager>(
        &log_task_runner_wrapper_, &uploader_, &profile_);
    FlushNonDelayedTasks();
  }

  void AddLogEntry(int extension_index) {
    ASSERT_GE(extension_index, 0);
    ASSERT_LT(extension_index, static_cast<int>(base::size(kExtensionIds)));
    const extensions::ExtensionId extension_id = kExtensionIds[extension_index];
    events_[extension_id].push_back(event_);
    manager_->Add({kExtensionIds[extension_index]}, event_);
    FlushNonDelayedTasks();
    event_.set_timestamp(event_.timestamp() + 1000);
  }

  void AddLogEntryForsetOfExtensions(
      const std::set<extensions::ExtensionId>& extensions) {
    for (const auto& extension_id : extensions) {
      events_[extension_id].push_back(event_);
    }
    manager_->Add(extensions, event_);
    FlushNonDelayedTasks();
    event_.set_timestamp(event_.timestamp() + 1000);
  }

  void AddLogEntryForAllExtensions() {
    AddLogEntryForsetOfExtensions(extension_ids_);
  }

  void ClearEventsDict() {
    base::DictionaryValue* mutable_dict;
    if (events_value_.GetAsDictionary(&mutable_dict))
      mutable_dict->Clear();
    else
      NOTREACHED();
  }

  void BuildReport() {
    base::Value event_list = ConvertEventsToValue(events_, /*profile=*/nullptr);
    base::Value context = reporting::GetContext(/*profile=*/nullptr);

    events_value_ = RealtimeReportingJobConfiguration::BuildReport(
        std::move(event_list), std::move(context));
  }

  void ExpectUploadAndCaptureCallback(
      reporting::MockReportQueue::EnqueueCallback* callback) {
    ClearEventsDict();
    BuildReport();

    EXPECT_CALL(*mock_report_queue_,
                AddRecord(MatchEvents(&events_value_), _, _))
        .WillOnce(
            Invoke([callback](base::StringPiece, reporting::Priority priority,
                              reporting::MockReportQueue::EnqueueCallback cb) {
              *callback = std::move(cb);
              return reporting::Status::StatusOK();
            }));
  }

  void ReportUploadSuccess(
      reporting::MockReportQueue::EnqueueCallback callback) {
    std::move(callback).Run(reporting::Status::StatusOK());
    FlushNonDelayedTasks();
  }

  void ExpectAndCompleteUpload() {
    ClearEventsDict();
    BuildReport();

    EXPECT_CALL(*mock_report_queue_,
                AddRecord(MatchEvents(&events_value_), _, _))
        .WillOnce(
            Invoke([](base::StringPiece, reporting::Priority priority,
                      reporting::MockReportQueue::EnqueueCallback callback) {
              std::move(callback).Run(reporting::Status::StatusOK());
              return reporting::Status::StatusOK();
            }));
  }

  void FlushNonDelayedTasks() {
    main_task_runner_->RunUntilIdle();
    while (log_task_runner_->HasPendingTask()) {
      log_task_runner_->RunUntilIdle();
      main_task_runner_->RunUntilIdle();
    }
  }

  void FastForwardTo(const base::TimeDelta& offset) {
    main_task_runner_->FastForwardBy(
        offset - (main_task_runner_->NowTicks() - base::TimeTicks()));
    FlushNonDelayedTasks();
  }

  void FastForwardUntilNoTasksRemain() {
    main_task_runner_->FastForwardUntilNoTasksRemain();
    while (log_task_runner_->HasPendingTask()) {
      log_task_runner_->RunUntilIdle();
      main_task_runner_->FastForwardUntilNoTasksRemain();
    }
  }

  void VerifyLogFile() {
    EXPECT_TRUE(base::PathExists(log_file_path_));
    ExtensionInstallEventLog log(log_file_path_);
    em::ExtensionInstallReportRequest log_events;
    log.Serialize(&log_events);
    EXPECT_TRUE(ContainsSameEvents(events_, log_events));
  }

  void VerifyAndDeleteLogFile() {
    VerifyLogFile();
    base::DeleteFile(log_file_path_);
  }

  TestLogTaskRunnerWrapper log_task_runner_wrapper_;
  content::BrowserTaskEnvironment task_environment_;
  extensions::QuotaService::ScopedDisablePurgeForTesting
      disable_purge_for_testing_;
  TestingProfile profile_;
  reporting::MockReportQueue* mock_report_queue_;
  ExtensionInstallEventLogUploader uploader_;
  std::unique_ptr<base::ScopedMockTimeMessageLoopTaskRunner>
      scoped_main_task_runner_;

  base::TestSimpleTaskRunner* log_task_runner_ = nullptr;
  base::TestMockTimeTaskRunner* main_task_runner_ = nullptr;

  const base::FilePath log_file_path_;
  const std::set<extensions::ExtensionId> extension_ids_;
  base::Value events_value_;
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      scoped_fake_statistics_provider_;

  em::ExtensionInstallReportLogEvent event_;
  Events events_;

  std::unique_ptr<ExtensionInstallEventLogManager> manager_;
};

// Create a manager with an empty log. Verify that no store is scheduled and no
// upload occurs.
TEST_F(ExtensionInstallEventLogManagerTest, CreateEmpty) {
  CreateManager();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Store a populated log. Create a manager that loads the non-empty log. Delete
// the log. Verify that no store is scheduled and an expedited initial upload
// occurs after fifteen minutes.
TEST_F(ExtensionInstallEventLogManagerTest, CreateNonEmpty) {
  ExtensionInstallEventLog log(log_file_path_);
  events_[kExtensionIds[0]].push_back(event_);
  log.Add(kExtensionIds[0], event_);
  log.Store();
  EXPECT_TRUE(base::PathExists(log_file_path_));

  CreateManager();
  ASSERT_TRUE(base::DeleteFile(log_file_path_));

  FastForwardTo(kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry after two minutes. Verify that a store is scheduled after
// five seconds and an expedited initial upload occurs after a total of fifteen
// minutes.
TEST_F(ExtensionInstallEventLogManagerTest, AddBeforeInitialUpload) {
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(2);
  FastForwardTo(offset);
  AddLogEntry(0 /* extension_index */);

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Add four log entries at two second cadence. Verify that
// stores are scheduled after five and eleven seconds and an upload occurs
// after three hours.
TEST_F(ExtensionInstallEventLogManagerTest, Add) {
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  AddLogEntry(0 /* extension_index */);

  FastForwardTo(offset + base::TimeDelta::FromSeconds(2));
  AddLogEntry(0 /* extension_index */);

  FastForwardTo(offset + base::TimeDelta::FromSeconds(4));
  AddLogEntry(0 /* extension_index */);

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + base::TimeDelta::FromSeconds(6));
  AddLogEntry(0 /* extension_index */);

  FastForwardTo(offset + base::TimeDelta::FromSeconds(6) + kStoreDelay -
                kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + base::TimeDelta::FromSeconds(6) + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kUploadInterval - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kUploadInterval);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Add an identical log entry for multiple extensions.
// Verify that a store is scheduled after five seconds and an upload occurs
// after three hours.
TEST_F(ExtensionInstallEventLogManagerTest, AddForMultipleExtensions) {
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  AddLogEntryForAllExtensions();

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kUploadInterval - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kUploadInterval);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Add an identical log entry for an empty set of
// extensions. Verify that no store is scheduled and no upload occurs.
TEST_F(ExtensionInstallEventLogManagerTest, AddForZeroExtensions) {
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  AddLogEntryForsetOfExtensions({});

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Fill the log for one extension until its size exceeds
// the threshold for expedited upload. Verify that a store is scheduled after
// five seconds and an upload occurs after fifteen minutes.
TEST_F(ExtensionInstallEventLogManagerTest, AddToTriggerMaxSizeExpedited) {
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  for (int i = 0; i <= kMaxSizeExpeditedUploadThreshold; ++i) {
    AddLogEntry(0 /* extension_index */);
  }

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Fill the logs for five extensions until their total size
// exceeds the threshold for expedited upload. Verify that a store is scheduled
// after five seconds and an upload occurs after fifteen minutes.
TEST_F(ExtensionInstallEventLogManagerTest, AddToTriggerTotalSizeExpedited) {
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  int i = 0;
  while (i <= kTotalSizeExpeditedUploadThreshold) {
    for (int j = 0; j < static_cast<int>(base::size(kExtensionIds)); ++i, ++j) {
      AddLogEntry(j /* extension_index */);
    }
  }

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Add an identical log entry for multiple extensions
// repeatedly, until the log size exceeds the threshold for expedited upload.
// Verify that a store is scheduled after five seconds and an upload occurs
// after fifteen minutes.
TEST_F(ExtensionInstallEventLogManagerTest,
       AddForMultipleExtensionsToTriggerTotalSizeExpedited) {
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  for (int i = 0; i <= kTotalSizeExpeditedUploadThreshold;
       i += base::size(kExtensionIds)) {
    AddLogEntryForAllExtensions();
  }

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry. Verify that a store is scheduled after five seconds and an
// expedited initial upload starts after fifteen minutes. Then, add another log
// entry. Complete the upload. Verify that the pending log entry is stored.
// Then, verify that a regular upload occurs three hours later.
TEST_F(ExtensionInstallEventLogManagerTest, RequestUploadAddUpload) {
  CreateManager();
  AddLogEntry(0 /* extension_index */);

  FastForwardTo(kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);

  reporting::MockReportQueue::EnqueueCallback upload_callback;
  ExpectUploadAndCaptureCallback(&upload_callback);
  FastForwardTo(kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  EXPECT_FALSE(base::PathExists(log_file_path_));

  AddLogEntry(0 /* extension_index */);
  ReportUploadSuccess(std::move(upload_callback));
  VerifyAndDeleteLogFile();

  FastForwardTo(kExpeditedUploadDelay + kUploadInterval - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(kExpeditedUploadDelay + kUploadInterval);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry. Verify that a store is scheduled after five seconds and an
// expedited initial upload starts after fifteen minutes. Then, fill the log for
// one extension until its size exceeds the threshold for expedited upload.
// Complete the upload. Verify that the pending log entries are stored. Then,
// verify that an expedited upload occurs fifteen minutes later.
TEST_F(ExtensionInstallEventLogManagerTest, RequestUploadAddExpeditedUpload) {
  CreateManager();
  AddLogEntry(0 /* extension_index */);

  FastForwardTo(kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);

  reporting::MockReportQueue::EnqueueCallback upload_callback;
  ExpectUploadAndCaptureCallback(&upload_callback);
  FastForwardTo(kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  EXPECT_FALSE(base::PathExists(log_file_path_));

  for (int i = 0; i <= kMaxSizeExpeditedUploadThreshold; ++i) {
    AddLogEntry(0 /* extension_index */);
  }
  ReportUploadSuccess(std::move(upload_callback));
  VerifyAndDeleteLogFile();

  FastForwardTo(kExpeditedUploadDelay + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(kExpeditedUploadDelay + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Fill the log for one extension until its size exceeds
// the threshold for expedited upload. Verify that a store is scheduled after
// five seconds and an upload starts after fifteen minutes. Then, add another
// log entry. Complete the upload. Verify that the pending log entry is stored.
// Then, verify that a regular upload occurs three hours later.
TEST_F(ExtensionInstallEventLogManagerTest, RequestExpeditedUploadAddUpload) {
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  for (int i = 0; i <= kMaxSizeExpeditedUploadThreshold; ++i) {
    AddLogEntry(0 /* extension_index */);
  }

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  reporting::MockReportQueue::EnqueueCallback upload_callback;
  ExpectUploadAndCaptureCallback(&upload_callback);
  FastForwardTo(offset + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  EXPECT_FALSE(base::PathExists(log_file_path_));

  AddLogEntry(0 /* extension_index */);
  ReportUploadSuccess(std::move(upload_callback));
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay + kUploadInterval - kOneMs);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kExpeditedUploadDelay + kUploadInterval);
  Mock::VerifyAndClearExpectations(mock_report_queue_);
  events_.clear();
  VerifyAndDeleteLogFile();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry. Destroy the manager. Verify that an immediate store is
// scheduled during destruction.
TEST_F(ExtensionInstallEventLogManagerTest, StoreOnShutdown) {
  CreateManager();

  AddLogEntry(0 /* extension_index */);

  manager_.reset();
  FlushNonDelayedTasks();
  VerifyAndDeleteLogFile();
}

// Store a populated log.  Clear all data related to the extension-install event
// log. Verify that an immediate deletion of the log file is scheduled.
TEST_F(ExtensionInstallEventLogManagerTest, Clear) {
  ExtensionInstallEventLog log(log_file_path_);
  events_[kExtensionIds[0]].push_back(event_);
  log.Add(kExtensionIds[0], event_);
  log.Store();
  ExtensionInstallEventLogManager::Clear(&log_task_runner_wrapper_, &profile_);
  VerifyLogFile();

  FlushNonDelayedTasks();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry. Destroy the manager. Verify that an immediate store is
// scheduled during destruction.  Clear all data related to the extension
// install event log. Create a manager. Verify that the log file is deleted
// before the manager attempts to load it.
TEST_F(ExtensionInstallEventLogManagerTest, RunClearRun) {
  CreateManager();

  AddLogEntry(0 /* extension_index */);

  manager_.reset();
  FlushNonDelayedTasks();
  VerifyLogFile();

  ExtensionInstallEventLogManager::Clear(&log_task_runner_wrapper_, &profile_);

  CreateManager();
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

}  // namespace policy
