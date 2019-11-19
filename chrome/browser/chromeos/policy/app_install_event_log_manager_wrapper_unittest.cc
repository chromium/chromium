// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_manager_wrapper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/policy/app_install_event_log.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
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
    FILE_PATH_LITERAL("app_push_install_log");

constexpr char kPackageName[] = "com.example.app";

class AppInstallEventLogManagerWrapperTestable
    : public AppInstallEventLogManagerWrapper {
 public:
  explicit AppInstallEventLogManagerWrapperTestable(Profile* profile)
      : AppInstallEventLogManagerWrapper(profile) {}

  scoped_refptr<base::SequencedTaskRunner> log_task_runner() {
    return log_task_runner_->GetTaskRunner();
  }

  // Make |Init()| visible for testing.
  using AppInstallEventLogManagerWrapper::Init;

  // AppInstallEventLogManagerWrapper:
  MOCK_METHOD0(CreateManager, void());
  MOCK_METHOD0(DestroyManager, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogManagerWrapperTestable);
};

}  // namespace

class AppInstallEventLogManagerWrapperTest : public testing::Test {
 protected:
  AppInstallEventLogManagerWrapperTest()
      : log_file_path_(profile_.GetPath().Append(kLogFileName)) {}

  // testing::Test:
  void SetUp() override { app_list_.AppendString(kPackageName); }

  void PopulateLogFileAndPrefs() {
    AppInstallEventLog log(log_file_path_);
    em::AppInstallReportLogEvent event;
    event.set_timestamp(0);
    event.set_event_type(em::AppInstallReportLogEvent::SUCCESS);
    log.Add(kPackageName, event);
    log.Store();
    profile_.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsRequested,
                             app_list_);
    profile_.GetPrefs()->Set(arc::prefs::kArcPushInstallAppsPending, app_list_);
  }

  void FlushPendingTasks() {
    base::RunLoop run_loop;
    ASSERT_TRUE(log_task_runner_);
    log_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  void CreateWrapper() {
    wrapper_ =
        std::make_unique<AppInstallEventLogManagerWrapperTestable>(&profile_);
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

  void VerifyLogFileAndPrefsNotCleared() {
    EXPECT_TRUE(base::PathExists(log_file_path_));
    EXPECT_EQ(app_list_, *profile_.GetPrefs()->GetList(
                             arc::prefs::kArcPushInstallAppsRequested));
    EXPECT_EQ(app_list_, *profile_.GetPrefs()->GetList(
                             arc::prefs::kArcPushInstallAppsPending));
  }

  void VerifyLogFileAndPrefsCleared() {
    EXPECT_FALSE(base::PathExists(log_file_path_));
    EXPECT_TRUE(profile_.GetPrefs()
                    ->FindPreference(arc::prefs::kArcPushInstallAppsRequested)
                    ->IsDefaultValue());
    EXPECT_TRUE(profile_.GetPrefs()
                    ->FindPreference(arc::prefs::kArcPushInstallAppsPending)
                    ->IsDefaultValue());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  const base::FilePath log_file_path_;
  base::ListValue app_list_;

  std::unique_ptr<AppInstallEventLogManagerWrapperTestable> wrapper_;

  scoped_refptr<base::SequencedTaskRunner> log_task_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogManagerWrapperTest);
};

// Populate a log file and the prefs holding the lists of apps for which
// push-install has been requested and is still pending. Enable reporting.
// Create a wrapper. Verify that a manager is created and neither the log file
// nor the prefs are cleared. Then, destroy the wrapper. Verify that neither the
// log file nor the prefs are cleared.
TEST_F(AppInstallEventLogManagerWrapperTest, EnableCreate) {
  PopulateLogFileAndPrefs();
  profile_.GetPrefs()->SetBoolean(prefs::kArcAppInstallEventLoggingEnabled,
                                  true);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateManager());
  EXPECT_CALL(*wrapper_, DestroyManager()).Times(0);
  InitWrapper();
  VerifyLogFileAndPrefsNotCleared();
  Mock::VerifyAndClearExpectations(&wrapper_);

  DestroyWrapper();
  VerifyLogFileAndPrefsNotCleared();
}

// Populate a log file and the prefs holding the lists of apps for which
// push-install has been requested and is still pending. Disable reporting.
// Create a wrapper. Verify that no manager is created and the log file and the
// prefs are cleared.
TEST_F(AppInstallEventLogManagerWrapperTest, DisableCreate) {
  PopulateLogFileAndPrefs();
  profile_.GetPrefs()->SetBoolean(prefs::kArcAppInstallEventLoggingEnabled,
                                  false);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateManager()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyManager());
  InitWrapper();
  VerifyLogFileAndPrefsCleared();
}

// Disable reporting. Create a wrapper. Verify that no manager is created. Then,
// enable reporting. Verify that a manager is created. Populate a log file and
// the prefs holding the lists of apps for which push-install has been requested
// and is still pending. Then, destroy the wrapper. Verify that neither the log
// file nor the prefs are cleared.
TEST_F(AppInstallEventLogManagerWrapperTest, CreateEnable) {
  profile_.GetPrefs()->SetBoolean(prefs::kArcAppInstallEventLoggingEnabled,
                                  false);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateManager()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyManager());
  InitWrapper();
  Mock::VerifyAndClearExpectations(&wrapper_);

  EXPECT_CALL(*wrapper_, CreateManager());
  EXPECT_CALL(*wrapper_, DestroyManager()).Times(0);
  profile_.GetPrefs()->SetBoolean(prefs::kArcAppInstallEventLoggingEnabled,
                                  true);
  Mock::VerifyAndClearExpectations(&wrapper_);
  FlushPendingTasks();

  PopulateLogFileAndPrefs();

  DestroyWrapper();
  VerifyLogFileAndPrefsNotCleared();
}

// Populate a log file and the prefs holding the lists of apps for which
// push-install has been requested and is still pending. Enable reporting.
// Create a wrapper. Verify that a manager is created and neither the log file
// nor the prefs are cleared. Then, disable reporting. Verify that the manager
// is destroyed and the log file and the prefs are cleared.
TEST_F(AppInstallEventLogManagerWrapperTest, CreateDisable) {
  PopulateLogFileAndPrefs();
  profile_.GetPrefs()->SetBoolean(prefs::kArcAppInstallEventLoggingEnabled,
                                  true);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateManager());
  EXPECT_CALL(*wrapper_, DestroyManager()).Times(0);
  InitWrapper();
  VerifyLogFileAndPrefsNotCleared();
  Mock::VerifyAndClearExpectations(&wrapper_);

  EXPECT_CALL(*wrapper_, CreateManager()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyManager());
  profile_.GetPrefs()->SetBoolean(prefs::kArcAppInstallEventLoggingEnabled,
                                  false);
  Mock::VerifyAndClearExpectations(&wrapper_);
  FlushPendingTasks();
  VerifyLogFileAndPrefsCleared();
}

}  // namespace policy
