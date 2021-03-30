// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_logger.h"

#include "base/files/file_path.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "chromeos/network/network_handler.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/value_builder.h"

using testing::_;
using testing::DoAll;
using testing::Mock;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kStatefulPath[] = "/tmp";
// The extension ids used here should be valid extension ids.
constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "bcdefghijklmnopabcdefghijklmnopa";
constexpr char kExtensionUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";  // URL of Chrome Web
                                                        // Store backend.
constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";

const int kTimestamp = 123456;

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

MATCHER_P(MatchEventExceptTimestamp, expected, "event matches") {
  em::ExtensionInstallReportLogEvent actual_event;
  actual_event.MergeFrom(arg);
  actual_event.clear_timestamp();

  em::ExtensionInstallReportLogEvent expected_event;
  expected_event.MergeFrom(expected);
  expected_event.clear_timestamp();

  return actual_event.SerializePartialAsString() ==
         expected_event.SerializePartialAsString();
}

MATCHER_P(MatchEventExceptDiskSpace, expected, "event matches") {
  em::ExtensionInstallReportLogEvent actual_event;
  actual_event.MergeFrom(arg);
  actual_event.clear_stateful_total();
  actual_event.clear_stateful_free();

  em::ExtensionInstallReportLogEvent expected_event;
  expected_event.MergeFrom(expected);
  expected_event.clear_stateful_total();
  expected_event.clear_stateful_free();

  return actual_event.SerializePartialAsString() ==
         expected_event.SerializePartialAsString();
}

MATCHER_P(MatchEventExceptTimestampAndDiskSpace, expected, "event matches") {
  em::ExtensionInstallReportLogEvent actual_event;
  actual_event.MergeFrom(arg);
  actual_event.clear_timestamp();
  actual_event.clear_stateful_total();
  actual_event.clear_stateful_free();

  em::ExtensionInstallReportLogEvent expected_event;
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

class MockExtensionInstallEventLoggerDelegate
    : public ExtensionInstallEventLogger::Delegate {
 public:
  MockExtensionInstallEventLoggerDelegate() = default;

  MOCK_METHOD2(Add,
               void(std::set<extensions::ExtensionId> extensions,
                    const em::ExtensionInstallReportLogEvent& event));
};

}  // namespace

class ExtensionInstallEventLoggerTest : public testing::Test {
 protected:
  ExtensionInstallEventLoggerTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        prefs_(profile_.GetTestingPrefService()),
        registry_(extensions::ExtensionRegistry::Get(&profile_)) {}

  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);

    chromeos::DBusThreadManager::Initialize();
    chromeos::PowerManagerClient::InitializeFake();

    chromeos::NetworkHandler::Initialize();
  }

  void TearDown() override {
    logger_.reset();
    task_environment_.RunUntilIdle();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void SetupForceList() {
    std::unique_ptr<base::Value> dict =
        extensions::DictionaryBuilder()
            .Set(kExtensionId1,
                 extensions::DictionaryBuilder()
                     .Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Set(kExtensionId2,
                 extensions::DictionaryBuilder()
                     .Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Build();
    prefs_->SetManagedPref(extensions::pref_names::kInstallForceList,
                           std::move(dict));
  }

  // Runs |function|, verifies that the expected event is added to the logs for
  // all apps in |extensions_| and its timestamp is set to the time at which the
  // |function| is run.
  template <typename T>
  void RunAndVerifyAdd(T function,
                       const std::set<extensions::ExtensionId>& extensions_) {
    Mock::VerifyAndClearExpectations(&delegate_);

    int64_t timestamp = 0;
    EXPECT_CALL(delegate_, Add(extensions_, MatchEventExceptTimestamp(event_)))
        .WillOnce(SaveTimestamp<1>(&timestamp));
    const int64_t before = GetCurrentTimestamp();
    function();
    const int64_t after = GetCurrentTimestamp();
    Mock::VerifyAndClearExpectations(&delegate_);

    EXPECT_LE(before, timestamp);
    EXPECT_GE(after, timestamp);
  }

  void CreateLogger() {
    logger_ = std::make_unique<ExtensionInstallEventLogger>(
        &delegate_, &profile_, registry_);
    event_.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestingPrefServiceSimple pref_service_;

  sync_preferences::TestingPrefServiceSyncable* prefs_;
  extensions::ExtensionRegistry* registry_;
  MockExtensionInstallEventLoggerDelegate delegate_;

  em::ExtensionInstallReportLogEvent event_;

  std::unique_ptr<ExtensionInstallEventLogger> logger_;
};

// Adds an event with a timestamp. Verifies that the event is added to the log
// and the timestamp is not changed.
TEST_F(ExtensionInstallEventLoggerTest, Add) {
  CreateLogger();

  event_.set_timestamp(kTimestamp);
  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->MergeFrom(event_);

  EXPECT_CALL(delegate_, Add(std::set<extensions::ExtensionId>{kExtensionId1},
                             MatchProto(event_)));
  logger_->Add(kExtensionId1, false /* gather_disk_space_info */,
               std::move(event));
}

// Adds an event without a timestamp. Verifies that the event is added to the
// log and the timestamp is set to the current time.
TEST_F(ExtensionInstallEventLoggerTest, AddSetsTimestamp) {
  CreateLogger();

  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->MergeFrom(event_);

  RunAndVerifyAdd(
      [&]() {
        logger_->Add(kExtensionId1, false /* gather_disk_space_info */,
                     std::move(event));
      },
      {kExtensionId1});
}

// Adds an event with a timestamp, requesting that disk space information be
// added to it. Verifies that a background task is posted that consults the disk
// mount manager. Then, verifies that after the background task has run, the
// event is added.
//
// It is not possible to test that disk size information is retrieved correctly
// as a mounted stateful partition cannot be simulated in unit tests.
TEST_F(ExtensionInstallEventLoggerTest, AddSetsDiskSpaceInfo) {
  CreateLogger();
  logger_->SetStatefulPathForTesting(base::FilePath(kStatefulPath));

  event_.set_timestamp(kTimestamp);
  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->MergeFrom(event_);
  event->clear_stateful_total();
  event->clear_stateful_free();

  EXPECT_CALL(delegate_, Add(_, _)).Times(0);
  logger_->Add(kExtensionId1, true /* gather_disk_space_info */,
               std::move(event));
  Mock::VerifyAndClearExpectations(&delegate_);

  int64_t stateful_total = 0;
  int64_t stateful_free = 0;
  EXPECT_CALL(delegate_, Add(std::set<extensions::ExtensionId>{kExtensionId1},
                             MatchEventExceptDiskSpace(event_)))
      .WillOnce(DoAll(SaveStatefulTotal<1>(&stateful_total),
                      SaveStatefulFree<1>(&stateful_free)));
  task_environment_.RunUntilIdle();

  EXPECT_GT(stateful_total, 0);
  EXPECT_GT(stateful_free, 0);
}

// Adds an event without a timestamp, requesting that disk space information be
// added to it. Verifies that a background task is posted that consults the disk
// mount manager. Then, verifies that after the background task has run, the
// event is added and its timestamp is set to the current time before posting
// the background task.
//
// It is not possible to test that disk size information is retrieved correctly
// as a mounted stateful partition cannot be simulated in unit tests.
TEST_F(ExtensionInstallEventLoggerTest, AddSetsTimestampAndDiskSpaceInfo) {
  CreateLogger();
  logger_->SetStatefulPathForTesting(base::FilePath(kStatefulPath));

  std::unique_ptr<em::ExtensionInstallReportLogEvent> event =
      std::make_unique<em::ExtensionInstallReportLogEvent>();
  event->MergeFrom(event_);
  event->clear_stateful_total();
  event->clear_stateful_free();

  EXPECT_CALL(delegate_, Add(_, _)).Times(0);
  const int64_t before = GetCurrentTimestamp();
  logger_->Add(kExtensionId1, true /* gather_disk_space_info */,
               std::move(event));
  const int64_t after = GetCurrentTimestamp();
  Mock::VerifyAndClearExpectations(&delegate_);

  int64_t timestamp = 0;
  int64_t stateful_total = 0;
  int64_t stateful_free = 0;
  EXPECT_CALL(delegate_, Add(std::set<extensions::ExtensionId>{kExtensionId1},
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

TEST_F(ExtensionInstallEventLoggerTest, UpdatePolicy) {
  auto* fake_user_manager = new ash::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
  user_manager::User* user =
      fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
          account_id, false /*is_affiliated*/, user_manager::USER_TYPE_REGULAR,
          &profile_);
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
  CreateLogger();
  logger_->SetStatefulPathForTesting(base::FilePath(kStatefulPath));

  SetupForceList();

  // Expected new extensions_ added with disk info.
  event_.set_event_type(em::ExtensionInstallReportLogEvent::POLICY_REQUEST);
  int64_t stateful_total = 0;
  int64_t stateful_free = 0;
  EXPECT_CALL(delegate_, Add(std::set<extensions::ExtensionId>{kExtensionId1,
                                                               kExtensionId2},
                             MatchEventExceptTimestampAndDiskSpace(event_)))
      .WillOnce(DoAll(SaveStatefulTotal<1>(&stateful_total),
                      SaveStatefulFree<1>(&stateful_free)));
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_GT(stateful_total, 0);
  EXPECT_GT(stateful_free, 0);
}

}  // namespace policy
