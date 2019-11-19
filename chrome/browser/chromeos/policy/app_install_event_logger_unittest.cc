// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_logger.h"

#include <stdint.h>

#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "chromeos/network/network_handler.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kStatefulMountPath[] = "/mnt/stateful_partition";
constexpr char kPackageName[] = "com.example.app";
constexpr char kPackageName2[] = "com.example.app2";
constexpr char kPackageName3[] = "com.example.app3";
constexpr char kPackageName4[] = "com.example.app4";
constexpr char kPackageName5[] = "com.example.app5";
const int kTimestamp = 123456;

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

ACTION_TEMPLATE(SaveTimestamp,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = testing::get<k>(args).timestamp();
}

int64_t GetCurrentTimestamp() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds();
}

class MockAppInstallEventLoggerDelegate
    : public AppInstallEventLogger::Delegate {
 public:
  MockAppInstallEventLoggerDelegate() = default;

  MOCK_METHOD2(Add,
               void(const std::set<std::string>& packages,
                    const em::AppInstallReportLogEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAppInstallEventLoggerDelegate);
};

void SetPolicy(policy::PolicyMap* map,
               const char* name,
               std::unique_ptr<base::Value> value) {
  map->Set(name, policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
           policy::POLICY_SOURCE_CLOUD, std::move(value), nullptr);
}

}  // namespace

class AppInstallEventLoggerTest : public testing::Test {
 protected:
  AppInstallEventLoggerTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);

    chromeos::DBusThreadManager::Initialize();
    chromeos::PowerManagerClient::InitializeFake();

    chromeos::NetworkHandler::Initialize();

    disk_mount_manager_ = new chromeos::disks::MockDiskMountManager;
    chromeos::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_);
    disk_mount_manager_->CreateDiskEntryForMountDevice(
        chromeos::disks::DiskMountManager::MountPointInfo(
            "/dummy/device/usb", kStatefulMountPath,
            chromeos::MOUNT_TYPE_DEVICE, chromeos::disks::MOUNT_CONDITION_NONE),
        "device_id", "device_label", "vendor", "product",
        chromeos::DEVICE_TYPE_UNKNOWN, 1 << 20 /* total_size_in_bytes */,
        false /* is_parent */, false /* has_media */, true /* on_boot_device */,
        true /* on_removable_device */, "ext4");
  }

  void TearDown() override {
    logger_.reset();
    task_environment_.RunUntilIdle();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::disks::DiskMountManager::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  // Runs |function|, verifies that the expected event is added to the logs for
  // all apps in |packages| and its timestamp is set to the time at which the
  // |function| is run.
  template <typename T>
  void RunAndVerifyAdd(T function, const std::set<std::string>& packages) {
    Mock::VerifyAndClearExpectations(&delegate_);

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
              std::make_unique<AppInstallEventLogger>(&delegate_, &profile_);
        },
        {});
    event_.set_event_type(em::AppInstallReportLogEvent::SUCCESS);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestingPrefServiceSimple pref_service_;

  // Owned by chromeos::disks::DiskMountManager.
  chromeos::disks::MockDiskMountManager* disk_mount_manager_ = nullptr;

  MockAppInstallEventLoggerDelegate delegate_;

  em::AppInstallReportLogEvent event_;

  std::unique_ptr<AppInstallEventLogger> logger_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLoggerTest);
};

// Store lists of apps for which push-install has been requested and is still
// pending. Clear all data related to app-install event log collection. Verify
// that the lists are cleared.
TEST_F(AppInstallEventLoggerTest, Clear) {
  base::ListValue list;
  list.AppendString("test");
  profile_.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsRequested, list);
  profile_.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsPending, list);
  AppInstallEventLogger::Clear(&profile_);
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

// Adds an event with a timestamp, requesting that disk space information be
// added to it. Verifies that a background task is posted that consults the disk
// mount manager. Then, verifies that after the background task has run, the
// event is added.
//
// It is not possible to test that disk size information is retrieved correctly
// as a mounted stateful partition cannot be simulated in unit tests.
TEST_F(AppInstallEventLoggerTest, AddSetsDiskSpaceInfo) {
  CreateLogger();

  event_.set_timestamp(kTimestamp);
  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->MergeFrom(event_);

  EXPECT_CALL(*disk_mount_manager_, disks()).Times(0);
  EXPECT_CALL(delegate_, Add(_, _)).Times(0);
  logger_->Add(kPackageName, true /* gather_disk_space_info */,
               std::move(event));
  Mock::VerifyAndClearExpectations(disk_mount_manager_);
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(*disk_mount_manager_, disks());
  EXPECT_CALL(delegate_,
              Add(std::set<std::string>{kPackageName}, MatchProto(event_)));
  task_environment_.RunUntilIdle();
}

// Adds an event without a timestamp, requesting that disk space information be
// added to it. Verifies that a background task is posted that consults the disk
// mount manager. Then, verifies that after the background task has run, the
// event is added and its timestamp is set to the current time before posting
// the background task.
//
// It is not possible to test that disk size information is retrieved correctly
// as a mounted stateful partition cannot be simulated in unit tests.
TEST_F(AppInstallEventLoggerTest, AddSetsTimestampAndDiskSpaceInfo) {
  CreateLogger();

  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  event->MergeFrom(event_);

  EXPECT_CALL(*disk_mount_manager_, disks()).Times(0);
  EXPECT_CALL(delegate_, Add(_, _)).Times(0);
  const int64_t before = GetCurrentTimestamp();
  logger_->Add(kPackageName, true /* gather_disk_space_info */,
               std::move(event));
  const int64_t after = GetCurrentTimestamp();
  Mock::VerifyAndClearExpectations(disk_mount_manager_);
  Mock::VerifyAndClearExpectations(&delegate_);

  int64_t timestamp = 0;
  EXPECT_CALL(*disk_mount_manager_, disks());
  EXPECT_CALL(delegate_, Add(std::set<std::string>{kPackageName},
                             MatchEventExceptTimestamp(event_)))
      .WillOnce(SaveTimestamp<1>(&timestamp));
  task_environment_.RunUntilIdle();

  EXPECT_LE(before, timestamp);
  EXPECT_GE(after, timestamp);
}

TEST_F(AppInstallEventLoggerTest, UpdatePolicy) {
  CreateLogger();

  policy::PolicyMap new_policy_map;

  base::DictionaryValue arc_policy;
  auto list = std::make_unique<base::ListValue>();

  // Test that REQUIRED, PREINSTALLED and FORCE_INSTALLED are markers to include
  // app to the tracking. BLOCKED and AVAILABLE are excluded.
  auto package1 = std::make_unique<base::DictionaryValue>();
  package1->SetString("installType", "REQUIRED");
  package1->SetString("packageName", kPackageName);
  list->Append(std::move(package1));
  auto package2 = std::make_unique<base::DictionaryValue>();
  package2->SetString("installType", "PREINSTALLED");
  package2->SetString("packageName", kPackageName2);
  list->Append(std::move(package2));
  auto package3 = std::make_unique<base::DictionaryValue>();
  package3->SetString("installType", "FORCE_INSTALLED");
  package3->SetString("packageName", kPackageName3);
  list->Append(std::move(package3));
  auto package4 = std::make_unique<base::DictionaryValue>();
  package4->SetString("installType", "BLOCKED");
  package4->SetString("packageName", kPackageName4);
  list->Append(std::move(package4));
  auto package5 = std::make_unique<base::DictionaryValue>();
  package5->SetString("installType", "AVAILABLE");
  package5->SetString("packageName", kPackageName5);
  list->Append(std::move(package5));
  arc_policy.SetList("applications", std::move(list));

  std::string arc_policy_string;
  base::JSONWriter::Write(arc_policy, &arc_policy_string);
  SetPolicy(&new_policy_map, key::kArcEnabled,
            std::make_unique<base::Value>(true));
  SetPolicy(&new_policy_map, key::kArcPolicy,
            std::make_unique<base::Value>(arc_policy_string));

  // Expected CANCELED with empty package set
  event_.set_event_type(em::AppInstallReportLogEvent::CANCELED);
  EXPECT_CALL(delegate_,
              Add(std::set<std::string>(), MatchEventExceptTimestamp(event_)));

  logger_->OnPolicyUpdated(policy::PolicyNamespace(),
                           policy::PolicyMap() /* previous */, new_policy_map);
  Mock::VerifyAndClearExpectations(&delegate_);

  // Expected new packages added with disk info.
  event_.set_event_type(em::AppInstallReportLogEvent::SERVER_REQUEST);
  EXPECT_CALL(delegate_, Add(std::set<std::string>{kPackageName, kPackageName3},
                             MatchEventExceptTimestamp(event_)));
  EXPECT_CALL(*disk_mount_manager_, disks());
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  // To avoid extra logging.
  g_browser_process->local_state()->SetBoolean(prefs::kWasRestarted, true);
}

}  // namespace policy
