// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_manager.h"

#include <iterator>
#include <map>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/app_install_event_log.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_uploader.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_util.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/quota_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::Invoke;
using testing::Mock;
using testing::Pointee;
using testing::SaveArg;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr base::FilePath::CharType kLogFileName[] =
    FILE_PATH_LITERAL("app_push_install_log");
constexpr base::TimeDelta kStoreDelay = base::TimeDelta::FromSeconds(5);
constexpr base::TimeDelta kUploadInterval = base::TimeDelta::FromHours(3);
constexpr base::TimeDelta kExpeditedUploadDelay =
    base::TimeDelta::FromMinutes(15);
constexpr base::TimeDelta kOneMs = base::TimeDelta::FromMilliseconds(1);

constexpr int kTotalSizeExpeditedUploadThreshold = 2048;
constexpr int kMaxSizeExpeditedUploadThreshold = 512;

constexpr char kDMToken[] = "token";
constexpr const char* kPackageNames[] = {"com.example.app1", "com.example.app2",
                                         "com.example.app3", "com.example.app4",
                                         "com.example.app5"};

using Events = std::map<std::string, std::vector<em::AppInstallReportLogEvent>>;

bool ContainsSameEvents(const Events& expected,
                        const em::AppInstallReportRequest& actual) {
  if (actual.app_install_reports_size() != static_cast<int>(expected.size())) {
    return false;
  }
  for (const auto& expected_app_log : expected) {
    bool app_found = false;
    for (int i = 0; i < actual.app_install_reports_size(); ++i) {
      const auto& actual_app_log = actual.app_install_reports(i);
      if (actual_app_log.package() == expected_app_log.first) {
        if (actual_app_log.logs_size() !=
            static_cast<int>(expected_app_log.second.size())) {
          return false;
        }
        for (int j = 0; j < static_cast<int>(expected_app_log.second.size());
             ++j) {
          if (actual_app_log.logs(j).SerializePartialAsString() !=
              expected_app_log.second[j].SerializePartialAsString()) {
            return false;
          }
        }
        app_found = true;
        break;
      }
    }
    if (!app_found) {
      return false;
    }
  }
  return true;
}

base::Value ConvertEventsToValue(const Events& events, Profile* profile) {
  base::Value event_list(base::Value::Type::LIST);

  for (auto it = events.begin(); it != events.end(); ++it) {
    const std::string& package = (*it).first;
    for (const em::AppInstallReportLogEvent& app_install_report_log_event :
         (*it).second) {
      base::Value wrapper;
      wrapper =
          ConvertEventToValue(package, app_install_report_log_event, profile);
      event_list.Append(std::move(wrapper));
    }
  }

  return event_list;
}

MATCHER_P(MatchEvents, expected, "contains events") {
  std::string arg_serialized_string;
  JSONStringValueSerializer arg_serializer(&arg_serialized_string);
  if (!arg_serializer.Serialize(arg))
    return false;

  DCHECK(expected);
  std::string expected_serialized_string;
  JSONStringValueSerializer expected_serializer(&expected_serialized_string);
  if (!expected_serializer.Serialize(*expected))
    return false;

  return arg_serialized_string == expected_serialized_string;
}

class TestLogTaskRunnerWrapper
    : public AppInstallEventLogManager::LogTaskRunnerWrapper {
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

  DISALLOW_COPY_AND_ASSIGN(TestLogTaskRunnerWrapper);
};

}  // namespace

class AppInstallEventLogManagerTest : public testing::Test {
 protected:
  AppInstallEventLogManagerTest()
      : uploader_(&cloud_policy_client_, /*profile=*/nullptr),
        log_task_runner_(log_task_runner_wrapper_.test_task_runner()),
        log_file_path_(profile_.GetPath().Append(kLogFileName)),
        packages_{std::begin(kPackageNames), std::end(kPackageNames)},
        events_value_(base::Value::Type::DICTIONARY),
        scoped_fake_statistics_provider_(
            std::make_unique<
                chromeos::system::ScopedFakeStatisticsProvider>()) {}

  // testing::Test:
  void SetUp() override {
    cloud_policy_client_.SetDMToken(kDMToken);

    event_.set_timestamp(0);
    event_.set_event_type(em::AppInstallReportLogEvent::SUCCESS);

    scoped_main_task_runner_ =
        std::make_unique<base::ScopedMockTimeMessageLoopTaskRunner>();
    main_task_runner_ = scoped_main_task_runner_->task_runner();
  }

  // testing::Test:
  void TearDown() override {
    Mock::VerifyAndClearExpectations(&cloud_policy_client_);
    EXPECT_CALL(cloud_policy_client_, CancelAppInstallReportUpload())
        .Times(AnyNumber());
    manager_.reset();
    FastForwardUntilNoTasksRemain();

    main_task_runner_ = nullptr;
    scoped_main_task_runner_.reset();
  }

  void CreateManager() {
    manager_ = std::make_unique<AppInstallEventLogManager>(
        &log_task_runner_wrapper_, &uploader_, &profile_);
    FlushNonDelayedTasks();
  }

  void AddLogEntry(int app_index) {
    ASSERT_GE(app_index, 0);
    ASSERT_LT(app_index, static_cast<int>(base::size(kPackageNames)));
    const std::string package_name = kPackageNames[app_index];
    events_[package_name].push_back(event_);
    manager_->Add({kPackageNames[app_index]}, event_);
    FlushNonDelayedTasks();
    event_.set_timestamp(event_.timestamp() + 1);
  }

  void AddLogEntryForsetOfApps(const std::set<std::string>& packages) {
    for (const auto& package_name : packages) {
      events_[package_name].push_back(event_);
    }
    manager_->Add(packages, event_);
    FlushNonDelayedTasks();
    event_.set_timestamp(event_.timestamp() + 1);
  }

  void AddLogEntryForAllApps() { AddLogEntryForsetOfApps(packages_); }

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

    AppendEventId(&event_list, context);
    events_value_ = RealtimeReportingJobConfiguration::BuildReport(
        std::move(event_list), std::move(context));
  }

  void ExpectUploadAndCaptureCallback(
      CloudPolicyClient::StatusCallback* callback) {
    ClearEventsDict();
    BuildReport();

    EXPECT_CALL(cloud_policy_client_,
                UploadRealtimeReport(MatchEvents(&events_value_), _))
        .WillOnce(SaveArg<1>(callback));
  }

  void ReportUploadSuccess(const CloudPolicyClient::StatusCallback& callback) {
    callback.Run(true /* success */);
    FlushNonDelayedTasks();
  }

  void ExpectAndCompleteUpload() {
    ClearEventsDict();
    BuildReport();

    EXPECT_CALL(cloud_policy_client_,
                UploadRealtimeReport(MatchEvents(&events_value_), _))
        .WillOnce(Invoke(
            [](base::Value, const CloudPolicyClient::StatusCallback& callback) {
              callback.Run(true /* success */);
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
    AppInstallEventLog log(log_file_path_);
    em::AppInstallReportRequest log_events;
    log.Serialize(&log_events);
    EXPECT_TRUE(ContainsSameEvents(events_, log_events));
  }

  void VerifyAndDeleteLogFile() {
    VerifyLogFile();
    base::DeleteFile(log_file_path_, false /* recursive */);
  }

  TestLogTaskRunnerWrapper log_task_runner_wrapper_;
  content::BrowserTaskEnvironment task_environment_;
  extensions::QuotaService::ScopedDisablePurgeForTesting
      disable_purge_for_testing_;
  TestingProfile profile_;
  MockCloudPolicyClient cloud_policy_client_;
  AppInstallEventLogUploader uploader_;
  std::unique_ptr<base::ScopedMockTimeMessageLoopTaskRunner>
      scoped_main_task_runner_;

  base::TestSimpleTaskRunner* log_task_runner_ = nullptr;
  base::TestMockTimeTaskRunner* main_task_runner_ = nullptr;

  const base::FilePath log_file_path_;
  const std::set<std::string> packages_;
  base::Value events_value_;
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      scoped_fake_statistics_provider_;

  em::AppInstallReportLogEvent event_;
  Events events_;

  std::unique_ptr<AppInstallEventLogManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogManagerTest);
};

// Create a manager with an empty log. Verify that no store is scheduled and no
// upload occurs.
TEST_F(AppInstallEventLogManagerTest, CreateEmpty) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Store a populated log. Create a manager that loads the non-empty log. Delete
// the log. Verify that no store is scheduled and an expedited initial upload
// occurs after fifteen minutes.
TEST_F(AppInstallEventLogManagerTest, CreateNonEmpty) {
  AppInstallEventLog log(log_file_path_);
  events_[kPackageNames[0]].push_back(event_);
  log.Add(kPackageNames[0], event_);
  log.Store();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();
  base::DeleteFile(log_file_path_, false /* recursive */);

  FastForwardTo(kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry after two minutes. Verify that a store is scheduled after
// five seconds and an expedited initial upload occurs after a total of fifteen
// minutes.
TEST_F(AppInstallEventLogManagerTest, AddBeforeInitialUpload) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(2);
  FastForwardTo(offset);
  AddLogEntry(0 /* app_index */);

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Add four log entries at two second cadence. Verify that
// stores are scheduled after five and eleven seconds and an upload occurs
// after three hours.
TEST_F(AppInstallEventLogManagerTest, Add) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  AddLogEntry(0 /* app_index */);

  FastForwardTo(offset + base::TimeDelta::FromSeconds(2));
  AddLogEntry(0 /* app_index */);

  FastForwardTo(offset + base::TimeDelta::FromSeconds(4));
  AddLogEntry(0 /* app_index */);

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + base::TimeDelta::FromSeconds(6));
  AddLogEntry(0 /* app_index */);

  FastForwardTo(offset + base::TimeDelta::FromSeconds(6) + kStoreDelay -
                kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + base::TimeDelta::FromSeconds(6) + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kUploadInterval - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kUploadInterval);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Add an identical log entry for multiple apps. Verify
// that a store is scheduled after five seconds and an upload occurs after three
// hours.
TEST_F(AppInstallEventLogManagerTest, AddForMultipleApps) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  AddLogEntryForAllApps();

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kUploadInterval - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kUploadInterval);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Add an identical log entry for an empty set of apps.
// Verify that no store is scheduled and no upload occurs.
TEST_F(AppInstallEventLogManagerTest, AddForZeroApps) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  AddLogEntryForsetOfApps({});

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Fill the log for one app until its size exceeds the
// threshold for expedited upload. Verify that a store is scheduled after five
// seconds and an upload occurs after fifteen minutes.
TEST_F(AppInstallEventLogManagerTest, AddToTriggerMaxSizeExpedited) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  for (int i = 0; i <= kMaxSizeExpeditedUploadThreshold; ++i) {
    AddLogEntry(0 /* app_index */);
  }

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Fill the logs for five apps until their total size
// exceeds the threshold for expedited upload. Verify that a store is scheduled
// after five seconds and an upload occurs after fifteen minutes.
TEST_F(AppInstallEventLogManagerTest, AddToTriggerTotalSizeExpedited) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  int i = 0;
  while (i <= kTotalSizeExpeditedUploadThreshold) {
    for (int j = 0; j < static_cast<int>(base::size(kPackageNames)); ++i, ++j) {
      AddLogEntry(j /* app_index */);
    }
  }

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Add an identical log entry for multiple apps repeatedly,
// until the log size exceeds the threshold for expedited upload. Verify that a
// store is scheduled after five seconds and an upload occurs after fifteen
// minutes.
TEST_F(AppInstallEventLogManagerTest,
       AddForMultipleAppsToTriggerTotalSizeExpedited) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  for (int i = 0; i <= kTotalSizeExpeditedUploadThreshold;
       i += base::size(kPackageNames)) {
    AddLogEntryForAllApps();
  }

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry. Verify that a store is scheduled after five seconds and an
// expedited initial upload starts after fifteen minutes. Then, add another log
// entry. Complete the upload. Verify that the pending log entry is stored.
// Then, verify that a regular upload occurs three hours later.
TEST_F(AppInstallEventLogManagerTest, RequestUploadAddUpload) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();
  AddLogEntry(0 /* app_index */);

  FastForwardTo(kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);

  CloudPolicyClient::StatusCallback upload_callback;
  ExpectUploadAndCaptureCallback(&upload_callback);
  FastForwardTo(kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  EXPECT_FALSE(base::PathExists(log_file_path_));

  AddLogEntry(0 /* app_index */);
  ReportUploadSuccess(upload_callback);
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardTo(kExpeditedUploadDelay + kUploadInterval - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(kExpeditedUploadDelay + kUploadInterval);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry. Verify that a store is scheduled after five seconds and an
// expedited initial upload starts after fifteen minutes. Then, fill the log for
// one app until its size exceeds the threshold for expedited upload. Complete
// the upload. Verify that the pending log entries are stored. Then, verify that
// an expedited upload occurs fifteen minutes later.
TEST_F(AppInstallEventLogManagerTest, RequestUploadAddExpeditedUpload) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();
  AddLogEntry(0 /* app_index */);

  FastForwardTo(kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);

  CloudPolicyClient::StatusCallback upload_callback;
  ExpectUploadAndCaptureCallback(&upload_callback);
  FastForwardTo(kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  EXPECT_FALSE(base::PathExists(log_file_path_));

  for (int i = 0; i <= kMaxSizeExpeditedUploadThreshold; ++i) {
    AddLogEntry(0 /* app_index */);
  }
  ReportUploadSuccess(upload_callback);
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardTo(kExpeditedUploadDelay + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(kExpeditedUploadDelay + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Wait twenty minutes. Fill the log for one app until its size exceeds the
// threshold for expedited upload. Verify that a store is scheduled after five
// seconds and an upload starts after fifteen minutes. Then, add another log
// entry. Complete the upload. Verify that the pending log entry is stored.
// Then, verify that a regular upload occurs three hours later.
TEST_F(AppInstallEventLogManagerTest, RequestExpeditedUploadAddUpload) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  const base::TimeDelta offset = base::TimeDelta::FromMinutes(20);
  FastForwardTo(offset);
  for (int i = 0; i <= kMaxSizeExpeditedUploadThreshold; ++i) {
    AddLogEntry(0 /* app_index */);
  }

  FastForwardTo(offset + kStoreDelay - kOneMs);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardTo(offset + kStoreDelay);
  VerifyAndDeleteLogFile();

  FastForwardTo(offset + kExpeditedUploadDelay - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  CloudPolicyClient::StatusCallback upload_callback;
  ExpectUploadAndCaptureCallback(&upload_callback);
  FastForwardTo(offset + kExpeditedUploadDelay);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  EXPECT_FALSE(base::PathExists(log_file_path_));

  AddLogEntry(0 /* app_index */);
  ReportUploadSuccess(upload_callback);
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardTo(offset + kExpeditedUploadDelay + kUploadInterval - kOneMs);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(base::PathExists(log_file_path_));

  ExpectAndCompleteUpload();
  FastForwardTo(offset + kExpeditedUploadDelay + kUploadInterval);
  Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  events_.clear();
  VerifyAndDeleteLogFile();

  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry. Destroy the manager. Verify that an immediate store is
// scheduled during destruction.
TEST_F(AppInstallEventLogManagerTest, StoreOnShutdown) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  AddLogEntry(0 /* app_index */);

  EXPECT_CALL(cloud_policy_client_, CancelAppInstallReportUpload());
  manager_.reset();
  FlushNonDelayedTasks();
  VerifyAndDeleteLogFile();
}

// Store a populated log. Populate the prefs holding the lists of apps for which
// push-install has been requested and is still pending. Clear all data related
// to the app-install event log. Verify that the prefs are cleared and an
// immediate deletion of the log file is scheduled.
TEST_F(AppInstallEventLogManagerTest, Clear) {
  AppInstallEventLog log(log_file_path_);
  events_[kPackageNames[0]].push_back(event_);
  log.Add(kPackageNames[0], event_);
  log.Store();

  base::ListValue list;
  list.AppendString("test");
  profile_.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsRequested, list);
  profile_.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsPending, list);

  AppInstallEventLogManager::Clear(&log_task_runner_wrapper_, &profile_);
  EXPECT_TRUE(profile_.GetPrefs()
                  ->FindPreference(arc::prefs::kArcPushInstallAppsRequested)
                  ->IsDefaultValue());
  EXPECT_TRUE(profile_.GetPrefs()
                  ->FindPreference(arc::prefs::kArcPushInstallAppsPending)
                  ->IsDefaultValue());
  VerifyLogFile();

  FlushNonDelayedTasks();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Add a log entry. Destroy the manager. Verify that an immediate store is
// scheduled during destruction. Then, populate the prefs holding the lists of
// apps for which push-install has been requested and is still pending. Clear
// all data related to the app-install event log. Verify that the prefs are
// cleared. Create a manager. Verify that the log file is deleted before the
// manager attempts to load it.
TEST_F(AppInstallEventLogManagerTest, RunClearRun) {
  EXPECT_CALL(cloud_policy_client_, UploadAppInstallReport(_, _)).Times(0);
  CreateManager();

  AddLogEntry(0 /* app_index */);

  EXPECT_CALL(cloud_policy_client_, CancelAppInstallReportUpload());
  manager_.reset();
  FlushNonDelayedTasks();
  VerifyLogFile();

  base::ListValue list;
  list.AppendString("test");
  profile_.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsRequested, list);
  profile_.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsPending, list);

  AppInstallEventLogManager::Clear(&log_task_runner_wrapper_, &profile_);
  EXPECT_TRUE(profile_.GetPrefs()
                  ->FindPreference(arc::prefs::kArcPushInstallAppsRequested)
                  ->IsDefaultValue());
  EXPECT_TRUE(profile_.GetPrefs()
                  ->FindPreference(arc::prefs::kArcPushInstallAppsPending)
                  ->IsDefaultValue());

  CreateManager();
  EXPECT_FALSE(base::PathExists(log_file_path_));

  FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

}  // namespace policy
