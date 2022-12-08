// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/cros_action_history/cros_action_recorder.h"

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_mock_clock_override.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

constexpr int kSecondsPerDay = 86400;
// Condition values start from this number for tests.
constexpr int kConditionValue = 11;

}  // namespace

// Test functions of CrOSActionRecorder.
class CrOSActionRecorderTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();

    model_dir_ =
        profile_->GetPath().AppendASCII(CrOSActionRecorder::kActionHistoryDir);
    download_filename_ =
        DownloadPrefs(profile_.get())
            .GetDefaultDownloadDirectoryForProfile()
            .AppendASCII(CrOSActionRecorder::kActionHistoryBasename);

    actions_ = {"Action0", "Action1", "Action2"};
    conditions_ = {"Condition0", "Condition1", "Condition2"};

    save_internal_secs_ = CrOSActionRecorder::kSaveInternal.InSeconds();
    // Set time_ to be base::Time::UnixEpoch().
    time_.Advance(base::Seconds(11612937600));
  }

  void TearDown() override {
    Test::TearDown();
    // Delete download_filename_ because it is put into a directory that may not
    // be deleted automatically.
    base::DeleteFile(download_filename_);
  }

  CrOSActionHistoryProto GetCrOSActionHistory() { return recorder_->actions_; }

  void SetCrOSActionRecorderType(const std::string& switch_str) {
    if (!switch_str.empty()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          ash::switches::kEnableCrOSActionRecorder, switch_str);
    }
    // Have to use base::WrapUnique instead of std::make_unique because the
    // constructor is private.
    recorder_ = base::WrapUnique(new CrOSActionRecorder(profile_.get()));

    Wait();
  }

  void SetDefaultFlag() { SetCrOSActionRecorderType(""); }

  void SetLogWithHash() {
    SetCrOSActionRecorderType(ash::switches::kCrOSActionRecorderWithHash);
  }

  void SetLogWithoutHash() {
    SetCrOSActionRecorderType(ash::switches::kCrOSActionRecorderWithoutHash);
  }

  void SetCopyToDownloadDir() {
    SetCrOSActionRecorderType(
        ash::switches::kCrOSActionRecorderCopyToDownloadDir);
  }

  void SetLogDisabled() {
    SetCrOSActionRecorderType(ash::switches::kCrOSActionRecorderDisabled);
  }

  CrOSActionHistoryProto ReadLog(const base::FilePath& action_file_path) {
    std::string proto_str;
    CHECK(base::ReadFileToString(action_file_path, &proto_str));
    CrOSActionHistoryProto actions_history;
    CHECK(actions_history.ParseFromString(proto_str));
    return actions_history;
  }

  // Read and Parse log from basename file.
  CrOSActionHistoryProto ReadLog(const std::string& basename) {
    return ReadLog(model_dir_.AppendASCII(basename));
  }

  void WriteLog(const CrOSActionHistoryProto& proto, const int day) {
    ASSERT_TRUE(base::PathExists(model_dir_) ||
                base::CreateDirectory(model_dir_));
    const base::FilePath action_file_path =
        model_dir_.AppendASCII(base::NumberToString(day));
    const std::string proto_str = proto.SerializeAsString();
    ASSERT_TRUE(base::WriteFile(action_file_path, proto_str));
  }

  // Expects |action| to be actions_[i], with certain features.
  void ExpectCrOSAction(const CrOSActionProto& action,
                        const int i,
                        const int64_t secs_since_epoch,
                        const bool should_hash = true) {
    EXPECT_EQ(action.action_name(),
              CrOSActionRecorder::MaybeHashed(actions_[i], should_hash));
    EXPECT_EQ(action.secs_since_epoch(), secs_since_epoch);
    EXPECT_EQ(action.conditions_size(), 1);
    const CrOSActionProto::CrOSActionConditionProto& condition =
        action.conditions(0);
    EXPECT_EQ(condition.name(),
              CrOSActionRecorder::MaybeHashed(conditions_[i], should_hash));
    EXPECT_EQ(condition.value(), i + kConditionValue);
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedMockClockOverride time_;
  std::unique_ptr<Profile> profile_;

  int64_t save_internal_secs_ = 0;
  base::FilePath model_dir_;
  base::FilePath download_filename_;
  std::vector<std::string> actions_;
  std::vector<std::string> conditions_;

  std::unique_ptr<CrOSActionRecorder> recorder_;
};

// Log is disabled by default.
TEST_F(CrOSActionRecorderTest, NoLogAsDefault) {
  SetDefaultFlag();
  recorder_->RecordAction({actions_[0]});
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
}

// Log is hashed if CrOSActionRecorderType == 1.
TEST_F(CrOSActionRecorderTest, HashActionNameAndConditionName) {
  SetLogWithHash();
  recorder_->RecordAction({actions_[0]}, {{conditions_[0], kConditionValue}});
  const CrOSActionHistoryProto& action_history = GetCrOSActionHistory();
  EXPECT_EQ(action_history.actions_size(), 1);

  ExpectCrOSAction(action_history.actions(0), 0, 0);
}

// Log action name and condition name explicitly if
// CrOSActionRecorderType == 2.
TEST_F(CrOSActionRecorderTest, DisableHashToLogExplicitly) {
  SetLogWithoutHash();
  recorder_->RecordAction({actions_[0]}, {{conditions_[0], kConditionValue}});
  const CrOSActionHistoryProto& action_history = GetCrOSActionHistory();
  EXPECT_EQ(action_history.actions_size(), 1);
  ExpectCrOSAction(action_history.actions(0), 0, 0, false);
}

// Check a new file is written every day for expected values.
TEST_F(CrOSActionRecorderTest, WriteToNewFileEveryDay) {
  SetLogWithHash();
  time_.Advance(base::Seconds(save_internal_secs_));
  recorder_->RecordAction({actions_[0]}, {{conditions_[0], kConditionValue}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the log to have correct values.
  const CrOSActionHistoryProto action_history_0 = ReadLog("0");
  EXPECT_EQ(action_history_0.actions_size(), 1);
  ExpectCrOSAction(action_history_0.actions(0), 0, save_internal_secs_);

  // Advance for 1 day.
  time_.Advance(base::Seconds(kSecondsPerDay));
  recorder_->RecordAction({actions_[1]},
                          {{conditions_[1], kConditionValue + 1}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the new log file to have correct values.
  const CrOSActionHistoryProto action_history_1 = ReadLog("1");
  EXPECT_EQ(action_history_1.actions_size(), 1);
  ExpectCrOSAction(action_history_1.actions(0), 1,
                   save_internal_secs_ + kSecondsPerDay);
}

// Check that the result is appended to previous log within a day.
TEST_F(CrOSActionRecorderTest, AppendToFileEverySaveInAday) {
  SetLogWithHash();
  time_.Advance(base::Seconds(save_internal_secs_));
  recorder_->RecordAction({actions_[0]}, {{conditions_[0], kConditionValue}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the log has correct values.
  const CrOSActionHistoryProto action_history_0 = ReadLog("0");
  EXPECT_EQ(action_history_0.actions_size(), 1);
  ExpectCrOSAction(action_history_0.actions(0), 0, save_internal_secs_);

  // Advance for 1 kSaveInternal.
  time_.Advance(base::Seconds(save_internal_secs_));
  recorder_->RecordAction({actions_[1]},
                          {{conditions_[1], kConditionValue + 1}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the log to have correct values (two actions).
  const CrOSActionHistoryProto action_history_1 = ReadLog("0");
  EXPECT_EQ(action_history_1.actions_size(), 2);
  ExpectCrOSAction(action_history_1.actions(0), 0, save_internal_secs_);
  ExpectCrOSAction(action_history_1.actions(1), 1, save_internal_secs_ * 2);

  // Advance for 3 kSaveInternal.
  time_.Advance(base::Seconds(save_internal_secs_ * 3));
  recorder_->RecordAction({actions_[2]},
                          {{conditions_[2], kConditionValue + 2}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the log to have correct values (three actions).
  const CrOSActionHistoryProto action_history_2 = ReadLog("0");
  EXPECT_EQ(action_history_2.actions_size(), 3);
  ExpectCrOSAction(action_history_1.actions(0), 0, save_internal_secs_);
  ExpectCrOSAction(action_history_1.actions(1), 1, save_internal_secs_ * 2);
  ExpectCrOSAction(action_history_2.actions(2), 2, save_internal_secs_ * 5);
}

// Check that the result is copied to cros-action-history.pb if enabled.
TEST_F(CrOSActionRecorderTest, CopyToDownloadDir) {
  // Create three CrOSActionHistoryProto.
  std::vector<CrOSActionHistoryProto> protos(3);
  for (int i = 0; i < 3; ++i) {
    auto& action = *protos[i].add_actions();
    action.set_action_name(actions_[i]);
    auto& condition = *action.add_conditions();
    condition.set_name(conditions_[i]);
    condition.set_value(i + kConditionValue);
  }

  // Write them into different files.
  WriteLog(protos[0], 2);
  WriteLog(protos[1], 13);
  WriteLog(protos[2], 23);

  SetCopyToDownloadDir();

  // Check they are merged into one file in the expected order.
  const CrOSActionHistoryProto action_history_merged =
      ReadLog(download_filename_);

  EXPECT_EQ(action_history_merged.actions_size(), 3);

  ExpectCrOSAction(action_history_merged.actions(0), 0, 0, false);
  ExpectCrOSAction(action_history_merged.actions(1), 1, 0, false);
  ExpectCrOSAction(action_history_merged.actions(2), 2, 0, false);

  // Check new data is not record if set to CopyToDownloadDir.
  recorder_->RecordAction({actions_[0]}, {{conditions_[0], kConditionValue}});
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
}

// Check that the result is not copied to cros-action-history.pb for
// kCrOSActionRecorderWithoutHash or kCrOSActionRecorderWithHash.
TEST_F(CrOSActionRecorderTest, LogToHomeDoesNotTriggerCopy) {
  CrOSActionHistoryProto proto1, proto2;
  auto& action1 = *proto1.add_actions();
  action1.set_action_name(actions_[1]);
  auto& condition1 = *action1.add_conditions();
  condition1.set_name(conditions_[1]);
  condition1.set_value(1 + kConditionValue);
  WriteLog(proto1, 1);

  SetLogWithoutHash();
  EXPECT_FALSE(base::PathExists(download_filename_));

  SetLogWithoutHash();
  EXPECT_FALSE(base::PathExists(download_filename_));
}

// Check SetLogDisabled removes everything in the cros action directory.
TEST_F(CrOSActionRecorderTest, DisableRemovesEverything) {
  CrOSActionHistoryProto proto1, proto2;
  auto& action1 = *proto1.add_actions();
  action1.set_action_name(actions_[1]);
  auto& condition1 = *action1.add_conditions();
  condition1.set_name(conditions_[1]);
  condition1.set_value(1 + kConditionValue);
  WriteLog(proto1, 1);

  SetLogDisabled();
  // model_dir_ should be removed.
  EXPECT_FALSE(base::PathExists(model_dir_));

  // There should be no record of actions.
  recorder_->RecordAction({actions_[0]});
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
}

}  // namespace app_list::test
