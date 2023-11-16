// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"

#include <stdint.h>

#include "ash/components/arc/arc_prefs.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::WithArgs;
namespace em = ::enterprise_management;

constexpr char kStatefulPath[] = "/tmp";
constexpr char kPackageName[] = "com.example.app";
constexpr char kPackageName2[] = "com.example.app2";
constexpr char kPackageName3[] = "com.example.app3";
constexpr char kPackageName4[] = "com.example.app4";
constexpr char kPackageName5[] = "com.example.app5";
const int kTimestamp = 123456;
const int64_t kAndroidId = 0x123456789ABCDEFL;

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

MATCHER_P(MatchEventExceptTimestamp, expected, "event matches") {
  em::AppInstallReportLogEvent actual_event;
  actual_event.MergeFrom(arg);
  actual_event.clear_timestamp();

  em::AppInstallReportLogEvent expected_event;
  expected_event.MergeFrom(expected);
  expected_event.clear_timestamp();

  return actual_event.SerializePartialAsString() ==
         expected_event.SerializePartialAsString();
}

MATCHER_P(MatchEventExceptDiskSpace, expected, "event matches") {
  em::AppInstallReportLogEvent actual_event;
  actual_event.MergeFrom(arg);
  actual_event.clear_stateful_total();
  actual_event.clear_stateful_free();

  em::AppInstallReportLogEvent expected_event;
  expected_event.MergeFrom(expected);
  expected_event.clear_stateful_total();
  expected_event.clear_stateful_free();

  return actual_event.SerializePartialAsString() ==
         expected_event.SerializePartialAsString();
}

MATCHER_P(MatchEventExceptTimestampAndDiskSpace, expected, "event matches") {
  em::AppInstallReportLogEvent actual_event;
  actual_event.MergeFrom(arg);
  actual_event.clear_timestamp();
  actual_event.clear_stateful_total();
  actual_event.clear_stateful_free();

  em::AppInstallReportLogEvent expected_event;
  expected_event.MergeFrom(expected);
  expected_event.clear_timestamp();
  expected_event.clear_stateful_total();
  expected_event.clear_stateful_free();

  return actual_event.SerializePartialAsString() ==
         expected_event.SerializePartialAsString();
}

ACTION_TEMPLATE(SaveTimestamp,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = testing::get<k>(args).timestamp();
}

ACTION_TEMPLATE(SaveAndroidId,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = testing::get<k>(args).android_id();
}

ACTION_TEMPLATE(SaveStatefulTotal,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = testing::get<k>(args).stateful_total();
}

ACTION_TEMPLATE(SaveStatefulFree,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = testing::get<k>(args).stateful_free();
}

int64_t GetCurrentTimestamp() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
}

class MockAppInstallEventLoggerDelegate
    : public ArcAppInstallEventLogger::Delegate {
 public:
  MockAppInstallEventLoggerDelegate() = default;

  MockAppInstallEventLoggerDelegate(const MockAppInstallEventLoggerDelegate&) =
      delete;
  MockAppInstallEventLoggerDelegate& operator=(
      const MockAppInstallEventLoggerDelegate&) = delete;

  void GetAndroidId(AndroidIdCallback callback) const override {
    GetAndroidId_(&callback);
  }

  MOCK_METHOD2(Add,
               void(const std::set<std::string>& packages,
                    const em::AppInstallReportLogEvent& event));
  MOCK_CONST_METHOD1(GetAndroidId_, void(AndroidIdCallback*));
};

class MockArcAppInstallPolicyDataHelper : public ArcAppInstallPolicyDataHelper {
 public:
  MockArcAppInstallPolicyDataHelper() = default;

  MockArcAppInstallPolicyDataHelper(const MockArcAppInstallPolicyDataHelper&) =
      delete;
  MockArcAppInstallPolicyDataHelper& operator=(
      const MockArcAppInstallPolicyDataHelper&) = delete;

  MOCK_METHOD2(AddPolicyData,
               void(const std::set<std::string>& current_pending,
                    std::int64_t num_apps_previously_installed));

  MOCK_METHOD(void, CheckForPolicyDataTimeout, ());

  MOCK_METHOD2(UpdatePolicySuccessRate,
               void(const std::string& package, bool success));

  MOCK_METHOD2(UpdatePolicySuccessRateForPackages,
               void(const std::set<std::string>& packages, bool success));
};

void SetPolicy(PolicyMap* map, const char* name, base::Value value) {
  map->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
           std::move(value), nullptr);
}

}  // namespace

class AppInstallEventLoggerTest : public testing::Test {
 protected:
  AppInstallEventLoggerTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  AppInstallEventLoggerTest(const AppInstallEventLoggerTest&) = delete;
  AppInstallEventLoggerTest& operator=(const AppInstallEventLoggerTest&) =
      delete;

  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);

    chromeos::PowerManagerClient::InitializeFake();
  }

  void TearDown() override {
    logger_.reset();
    task_environment_.RunUntilIdle();
    chromeos::PowerManagerClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  // Runs |function|, verifies that the expected event is added to the logs for
  // all apps in |packages| and its timestamp is set to the time at which the
  // |function| is run.
  template <typename T>
  void RunAndVerifyAdd(T function, const std::set<std::string>& packages) {
    Mock::VerifyAndClearExpectations(&delegate_);

    SetAndroidId(0L);
    int64_t timestamp = 0;
    EXPECT_CALL(delegate_, Add(packages, MatchEventExceptTimestamp(event_)))
        .WillOnce(SaveTimestamp<1>(&timestamp));
    const int64_t before = GetCurrentTimestamp();
    function();
    const int64_t after = GetCurrentTimestamp();
    Mock::VerifyAndClearExpectations(&delegate_);

    EXPECT_LE(before, timestamp);
    EXPECT_GE(after, timestamp);
  }

  void CreateLogger() {
    event_.set_event_type(em::AppInstallReportLogEvent::CANCELED);
    RunAndVerifyAdd(
        [&]() {
          logger_ =
              std::make_unique<ArcAppInstallEventLogger>(&delegate_, &profile_);
        },
        {});
    event_.set_event_type(em::AppInstallReportLogEvent::SUCCESS);
  }

  void SetAndroidId(int64_t android_id) {
    if (android_id) {
      event_.set_android_id(android_id);
    } else {
      event_.clear_android_id();
    }
    EXPECT_CALL(delegate_, GetAndroidId_(_))
        .WillOnce(WithArgs<0>(
            Invoke([=](ArcAppInstallEventLogger::Delegate::AndroidIdCallback*
                           callback) {
              std::move(*callback).Run(android_id, kAndroidId);
            })));
  }

  PolicyMap CreatePolicyWithForceInstalls(std::set<std::string> package_names) {
    PolicyMap policy_map;

    base::Value::Dict arc_policy;
    base::Value::List list;

    for (std::string package_name : package_names) {
      base::Value::Dict package;
      package.Set("installType", "FORCE_INSTALLED");
      package.Set("packageName", package_name);
      list.Append(std::move(package));
    }

    arc_policy.Set("applications", std::move(list));
    std::string arc_policy_string;
    base::JSONWriter::Write(arc_policy, &arc_policy_string);
    SetPolicy(&policy_map, key::kArcEnabled, base::Value(true));
    SetPolicy(&policy_map, key::kArcPolicy, base::Value(arc_policy_string));

    return policy_map;
  }

  base::Value CreateComplianceReport(
      std::set<std::string> noncompliant_packages) {
    base::Value::List details;

    for (std::string package_name : noncompliant_packages) {
      base::Value::Dict package;
      package.Set("nonComplianceReason", 5);
      package.Set("packageName", package_name);
      details.Append(std::move(package));
    }

    base::Value::Dict compliance_report;
    compliance_report.Set("nonComplianceDetails", std::move(details));
    return base::Value(std::move(compliance_report));
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
  TestingPrefServiceSimple pref_service_;
  TestingProfile profile_;

  MockAppInstallEventLoggerDelegate delegate_;

  MockArcAppInstallPolicyDataHelper policy_data_helper_;

  em::AppInstallReportLogEvent event_;

  std::unique_ptr<ArcAppInstallEventLogger> logger_;
};

// Store lists of apps for which push-install has been requested and is still
// pending. Clear all data related to app-install event log collection. Verify
// that the lists are cleared.
TEST_F(AppInstallEventLoggerTest, Clear) {
  base::Value::List list;
  list.Append("test");
  profile_.GetPrefs()->SetList(arc::prefs::kArcPushInstallAppsRequested,
                               list.Clone());
  profile_.GetPrefs()->SetList(arc::prefs::kArcPushInstallAppsPending,
                               list.Clone());
  ArcAppInstallEventLogger::Clear(&profile_);
  EXPECT_TRUE(profile_.GetPrefs()
                  ->FindPreference(arc::prefs::kArcPushInstallAppsRequested)
                  ->IsDefaultValue());
  EXPECT_TRUE(profile_.GetPrefs()
                  ->FindPreference(arc::prefs::kArcPushInstallAppsPending)
                  ->IsDefaultValue());
}

// Adds an event with a timestamp. Verifies that the event is added to the log
// and the timestamp is not changed.
TEST_F(AppInstallEventLoggerTest, Add) {
  CreateLogger();

  event_.set_timestamp(kTimestamp);
  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->MergeFrom(event_);

  SetAndroidId(kAndroidId);
  EXPECT_CALL(delegate_,
              Add(std::set<std::string>{kPackageName}, MatchProto(event_)));
  logger_->Add(kPackageName, false /* gather_disk_space_info */,
               std::move(event));
}

// Adds an event without a timestamp. Verifies that the event is added to the
// log and the timestamp is set to the current time.
TEST_F(AppInstallEventLoggerTest, AddSetsTimestamp) {
  CreateLogger();

  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->MergeFrom(event_);

  RunAndVerifyAdd(
      [&]() {
        logger_->Add(kPackageName, false /* gather_disk_space_info */,
                     std::move(event));
      },
      {kPackageName});
}

// If an android id is available, verify it is added to the event.
TEST_F(AppInstallEventLoggerTest, AddsAndroidId) {
  CreateLogger();

  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->MergeFrom(event_);
  event->clear_android_id();

  SetAndroidId(kAndroidId);
  int64_t android_id = 0;
  EXPECT_CALL(delegate_, Add(std::set<std::string>{kPackageName},
                             MatchEventExceptTimestamp(event_)))
      .WillOnce(SaveAndroidId<1>(&android_id));
  logger_->Add(kPackageName, false /* gather_disk_space_info */,
               std::move(event));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kAndroidId, android_id);
}

// If an android id isn't available, then the proto field should not be set.
TEST_F(AppInstallEventLoggerTest, DoesNotAddsAndroidId) {
  CreateLogger();

  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->MergeFrom(event_);
  event->clear_android_id();

  SetAndroidId(0);
  int64_t android_id = -1L;
  EXPECT_CALL(delegate_, Add(std::set<std::string>{kPackageName},
                             MatchEventExceptTimestamp(event_)))
      .WillOnce(SaveAndroidId<1>(&android_id));
  logger_->Add(kPackageName, false /* gather_disk_space_info */,
               std::move(event));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, android_id);
}

// Adds an event with a timestamp, requesting that disk space information be
// added to it. Verifies that after the background task has run, the event is
// added with valid disk space info.
TEST_F(AppInstallEventLoggerTest, AddSetsDiskSpaceInfo) {
  CreateLogger();
  logger_->SetStatefulPathForTesting(base::FilePath(kStatefulPath));

  event_.set_timestamp(kTimestamp);
  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->MergeFrom(event_);
  event->clear_stateful_total();
  event->clear_stateful_free();

  EXPECT_CALL(delegate_, Add(_, _)).Times(0);
  logger_->Add(kPackageName, true /* gather_disk_space_info */,
               std::move(event));
  Mock::VerifyAndClearExpectations(&delegate_);

  int64_t stateful_total = 0;
  int64_t stateful_free = 0;
  SetAndroidId(kAndroidId);
  EXPECT_CALL(delegate_, Add(std::set<std::string>{kPackageName},
                             MatchEventExceptDiskSpace(event_)))
      .WillOnce(DoAll(SaveStatefulTotal<1>(&stateful_total),
                      SaveStatefulFree<1>(&stateful_free)));
  task_environment_.RunUntilIdle();

  EXPECT_GT(stateful_total, 0);
  EXPECT_GT(stateful_free, 0);
}

// Adds an event without a timestamp, requesting that disk space information be
// added to it. Verifies that after the background task has run, the event is
// added with valid disk space info and its timestamp is set to the current time
// before posting the background task.
TEST_F(AppInstallEventLoggerTest, AddSetsTimestampAndDiskSpaceInfo) {
  CreateLogger();
  logger_->SetStatefulPathForTesting(base::FilePath(kStatefulPath));

  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->MergeFrom(event_);
  event->clear_stateful_total();
  event->clear_stateful_free();

  EXPECT_CALL(delegate_, Add(_, _)).Times(0);
  const int64_t before = GetCurrentTimestamp();
  logger_->Add(kPackageName, true /* gather_disk_space_info */,
               std::move(event));
  const int64_t after = GetCurrentTimestamp();
  Mock::VerifyAndClearExpectations(&delegate_);

  int64_t timestamp = 0;
  int64_t stateful_total = 0;
  int64_t stateful_free = 0;
  SetAndroidId(kAndroidId);
  EXPECT_CALL(delegate_, Add(std::set<std::string>{kPackageName},
                             MatchEventExceptTimestampAndDiskSpace(event_)))
      .WillOnce(DoAll(SaveTimestamp<1>(&timestamp),
                      SaveStatefulTotal<1>(&stateful_total),
                      SaveStatefulFree<1>(&stateful_free)));
  task_environment_.RunUntilIdle();

  EXPECT_GT(stateful_total, 0);
  EXPECT_GT(stateful_free, 0);
  EXPECT_LE(before, timestamp);
  EXPECT_GE(after, timestamp);
}

TEST_F(AppInstallEventLoggerTest, UpdatePolicy) {
  CreateLogger();
  logger_->SetStatefulPathForTesting(base::FilePath(kStatefulPath));

  PolicyMap new_policy_map;

  base::Value::Dict arc_policy;
  base::Value::List list;

  // Test that REQUIRED, PREINSTALLED and FORCE_INSTALLED are markers to include
  // app to the tracking. BLOCKED and AVAILABLE are excluded.
  base::Value::Dict package1;
  package1.Set("installType", "REQUIRED");
  package1.Set("packageName", kPackageName);
  list.Append(std::move(package1));
  base::Value::Dict package2;
  package2.Set("installType", "PREINSTALLED");
  package2.Set("packageName", kPackageName2);
  list.Append(std::move(package2));
  base::Value::Dict package3;
  package3.Set("installType", "FORCE_INSTALLED");
  package3.Set("packageName", kPackageName3);
  list.Append(std::move(package3));
  base::Value::Dict package4;
  package4.Set("installType", "BLOCKED");
  package4.Set("packageName", kPackageName4);
  list.Append(std::move(package4));
  base::Value::Dict package5;
  package5.Set("installType", "AVAILABLE");
  package5.Set("packageName", kPackageName5);
  list.Append(std::move(package5));
  arc_policy.Set("applications", std::move(list));

  std::string arc_policy_string;
  base::JSONWriter::Write(arc_policy, &arc_policy_string);
  SetPolicy(&new_policy_map, key::kArcEnabled, base::Value(true));
  SetPolicy(&new_policy_map, key::kArcPolicy, base::Value(arc_policy_string));

  // Expected CANCELED with empty package set
  event_.set_event_type(em::AppInstallReportLogEvent::CANCELED);
  SetAndroidId(kAndroidId);
  EXPECT_CALL(delegate_,
              Add(std::set<std::string>(), MatchEventExceptTimestamp(event_)));

  logger_->OnPolicyUpdated(PolicyNamespace(), /*previous=*/PolicyMap(),
                           new_policy_map);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Expected new packages added with disk info.
  event_.set_event_type(em::AppInstallReportLogEvent::SERVER_REQUEST);
  int64_t stateful_total = 0;
  int64_t stateful_free = 0;
  SetAndroidId(kAndroidId);
  EXPECT_CALL(delegate_, Add(std::set<std::string>{kPackageName, kPackageName3},
                             MatchEventExceptTimestampAndDiskSpace(event_)))
      .WillOnce(DoAll(SaveStatefulTotal<1>(&stateful_total),
                      SaveStatefulFree<1>(&stateful_free)));
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_GT(stateful_total, 0);
  EXPECT_GT(stateful_free, 0);

  // To avoid extra logging.
  g_browser_process->local_state()->SetBoolean(prefs::kWasRestarted, true);
}

TEST_F(AppInstallEventLoggerTest, PolicySuccessRate_AddPolicyData) {
  CreateLogger();
  logger_->SetStatefulPathForTesting(base::FilePath(kStatefulPath));
  PolicyMap policy = CreatePolicyWithForceInstalls({kPackageName});

  logger_->OnPolicyUpdated(PolicyNamespace(), /* previous */ PolicyMap(),
                           policy);
  ON_CALL(policy_data_helper_, UpdatePolicySuccessRateForPackages);
  ON_CALL(policy_data_helper_, AddPolicyData);
}

TEST_F(AppInstallEventLoggerTest, PolicySuccessRate_UpdatePolicySuccessRate) {
  CreateLogger();
  logger_->SetStatefulPathForTesting(base::FilePath(kStatefulPath));
  logger_->UpdatePolicySuccessRate(kPackageName, true);
  ON_CALL(policy_data_helper_, UpdatePolicySuccessRate);
}

}  // namespace policy
