// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_manager_wrapper.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/policy/extension_install_event_log.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr base::FilePath::CharType kLogFileName[] =
    FILE_PATH_LITERAL("extension_install_log");

constexpr char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

class ExtensionInstallEventLogManagerWrapperTestable
    : public ExtensionInstallEventLogManagerWrapper {
 public:
  explicit ExtensionInstallEventLogManagerWrapperTestable(Profile* profile)
      : ExtensionInstallEventLogManagerWrapper(profile) {}

  scoped_refptr<base::SequencedTaskRunner> log_task_runner() {
    return log_task_runner_->GetTaskRunner();
  }

  // Make |Init()| visible for testing.
  using ExtensionInstallEventLogManagerWrapper::Init;

  // ExtensionInstallEventLogManagerWrapper:
  MOCK_METHOD0(CreateManager, void());
  MOCK_METHOD0(DestroyManager, void());
};

}  // namespace

class ExtensionInstallEventLogManagerWrapperTest : public testing::Test {
 protected:
  ExtensionInstallEventLogManagerWrapperTest()
      : log_file_path_(profile_.GetPath().Append(kLogFileName)) {}

  void PopulateLogFile() {
    ExtensionInstallEventLog log(log_file_path_);
    em::ExtensionInstallReportLogEvent event;
    event.set_timestamp(0);
    event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
    log.Add(kExtensionId, event);
    log.Store();
  }

  void FlushPendingTasks() {
    base::RunLoop run_loop;
    ASSERT_TRUE(log_task_runner_);
    log_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  void CreateWrapper() {
    wrapper_ = std::make_unique<ExtensionInstallEventLogManagerWrapperTestable>(
        &profile_);
    log_task_runner_ = wrapper_->log_task_runner();
  }

  void DestroyWrapper() {
    wrapper_.reset();
    FlushPendingTasks();
    log_task_runner_ = nullptr;
  }

  void InitWrapper() {
    ASSERT_TRUE(wrapper_);
    wrapper_->Init();
    FlushPendingTasks();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  const base::FilePath log_file_path_;

  std::unique_ptr<ExtensionInstallEventLogManagerWrapperTestable> wrapper_;

  scoped_refptr<base::SequencedTaskRunner> log_task_runner_;
};

// Populate a log file. Enable reporting. Create a wrapper. Verify that a
// manager is created and the log file is not cleared. Then, destroy the
// wrapper. Verify that the log file is not cleared.
TEST_F(ExtensionInstallEventLogManagerWrapperTest, EnableCreate) {
  PopulateLogFile();
  profile_.GetPrefs()->SetBoolean(prefs::kExtensionInstallEventLoggingEnabled,
                                  true);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateManager());
  EXPECT_CALL(*wrapper_, DestroyManager()).Times(0);
  InitWrapper();
  // Verify that the log file is not cleared.
  EXPECT_TRUE(base::PathExists(log_file_path_));
  Mock::VerifyAndClearExpectations(&wrapper_);

  DestroyWrapper();
  // Verify that the log file is not cleared.
  EXPECT_TRUE(base::PathExists(log_file_path_));
}

// Populate a log file. Disable reporting. Create a wrapper. Verify that no
// manager is created and the log file is cleared.
TEST_F(ExtensionInstallEventLogManagerWrapperTest, DisableCreate) {
  PopulateLogFile();
  profile_.GetPrefs()->SetBoolean(prefs::kExtensionInstallEventLoggingEnabled,
                                  false);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateManager()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyManager());
  InitWrapper();
  // Verify that the log file is cleared.
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

// Disable reporting. Create a wrapper. Verify that no manager is created. Then,
// enable reporting. Verify that a manager is created. Populate a log file.
// Then, destroy the wrapper. Verify that the log file is not cleared.
TEST_F(ExtensionInstallEventLogManagerWrapperTest, CreateEnable) {
  profile_.GetPrefs()->SetBoolean(prefs::kExtensionInstallEventLoggingEnabled,
                                  false);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateManager()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyManager());
  InitWrapper();
  Mock::VerifyAndClearExpectations(&wrapper_);

  EXPECT_CALL(*wrapper_, CreateManager());
  EXPECT_CALL(*wrapper_, DestroyManager()).Times(0);
  profile_.GetPrefs()->SetBoolean(prefs::kExtensionInstallEventLoggingEnabled,
                                  true);
  Mock::VerifyAndClearExpectations(&wrapper_);
  FlushPendingTasks();

  PopulateLogFile();

  DestroyWrapper();
  // Verify that the log file is not cleared.
  EXPECT_TRUE(base::PathExists(log_file_path_));
}

// Populate a log file. Enable reporting. Create a wrapper. Verify that a
// manager is created and the log file is not cleared. Then, disable reporting.
// Verify that the manager is destroyed and the log file is cleared.
TEST_F(ExtensionInstallEventLogManagerWrapperTest, CreateDisable) {
  PopulateLogFile();
  profile_.GetPrefs()->SetBoolean(prefs::kExtensionInstallEventLoggingEnabled,
                                  true);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateManager());
  EXPECT_CALL(*wrapper_, DestroyManager()).Times(0);
  InitWrapper();
  // Verify that the log file is not cleared.
  EXPECT_TRUE(base::PathExists(log_file_path_));
  Mock::VerifyAndClearExpectations(&wrapper_);

  EXPECT_CALL(*wrapper_, CreateManager()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyManager());
  profile_.GetPrefs()->SetBoolean(prefs::kExtensionInstallEventLoggingEnabled,
                                  false);
  Mock::VerifyAndClearExpectations(&wrapper_);
  FlushPendingTasks();
  // Verify that the log file is cleared.
  EXPECT_FALSE(base::PathExists(log_file_path_));
}

}  // namespace policy
