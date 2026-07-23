// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/app_install_event_log_manager_wrapper.h"

#include "ash/constants/ash_policy_pref_names.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kPackageName[] = "com.example.app";

class AppInstallEventLogManagerWrapperTestable
    : public AppInstallEventLogManagerWrapper {
 public:
  AppInstallEventLogManagerWrapperTestable(PrefService* local_state,
                                           Profile* profile)
      : AppInstallEventLogManagerWrapper(local_state, profile) {}

  AppInstallEventLogManagerWrapperTestable(
      const AppInstallEventLogManagerWrapperTestable&) = delete;
  AppInstallEventLogManagerWrapperTestable& operator=(
      const AppInstallEventLogManagerWrapperTestable&) = delete;


  // Make |Init()| visible for testing.
  using AppInstallEventLogManagerWrapper::Init;

  // AppInstallEventLogManagerWrapper:
  MOCK_METHOD(void, CreateEncryptedReporter, (), (override));
  MOCK_METHOD(void, DestroyEncryptedReporter, (), (override));
};

}  // namespace

class AppInstallEventLogManagerWrapperTest : public testing::Test {
 protected:
  AppInstallEventLogManagerWrapperTest() = default;

  AppInstallEventLogManagerWrapperTest(
      const AppInstallEventLogManagerWrapperTest&) = delete;
  AppInstallEventLogManagerWrapperTest& operator=(
      const AppInstallEventLogManagerWrapperTest&) = delete;

  // testing::Test:
  void SetUp() override {
    app_list_.Append(kPackageName);
  }

  void PopulatePrefs() {
    profile_.GetPrefs()->SetList(arc::prefs::kArcPushInstallAppsRequested,
                                 app_list_.Clone());
    profile_.GetPrefs()->SetList(arc::prefs::kArcPushInstallAppsPending,
                                 app_list_.Clone());
  }

  void CreateWrapper() {
    wrapper_ = std::make_unique<AppInstallEventLogManagerWrapperTestable>(
        TestingBrowserProcess::GetGlobal()->local_state(), &profile_);
  }

  void DestroyWrapper() {
    wrapper_.reset();
  }

  void InitWrapper() {
    ASSERT_TRUE(wrapper_);
    wrapper_->Init();
  }

  void VerifyPrefsNotCleared() {
    EXPECT_EQ(app_list_, profile_.GetPrefs()->GetList(
                             arc::prefs::kArcPushInstallAppsRequested));
    EXPECT_EQ(app_list_, profile_.GetPrefs()->GetList(
                             arc::prefs::kArcPushInstallAppsPending));
  }

  void VerifyPrefsCleared() {
    EXPECT_TRUE(profile_.GetPrefs()
                    ->FindPreference(arc::prefs::kArcPushInstallAppsRequested)
                    ->IsDefaultValue());
    EXPECT_TRUE(profile_.GetPrefs()
                    ->FindPreference(arc::prefs::kArcPushInstallAppsPending)
                    ->IsDefaultValue());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  base::ListValue app_list_;

  ash::SessionTerminationManager session_termination_manager_;
  std::unique_ptr<AppInstallEventLogManagerWrapperTestable> wrapper_;
};

// Populate the prefs holding the lists of apps for which push-install has been
// requested and is still pending. Enable reporting. Create a wrapper. Verify
// that a reporter is created and the prefs are not cleared. Then, destroy the
// wrapper. Verify that the prefs are still not cleared.
TEST_F(AppInstallEventLogManagerWrapperTest, EnableCreate) {
  PopulatePrefs();
  profile_.GetPrefs()->SetBoolean(ash::prefs::kArcAppInstallEventLoggingEnabled,
                                  true);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateEncryptedReporter());
  EXPECT_CALL(*wrapper_, DestroyEncryptedReporter()).Times(0);
  InitWrapper();
  VerifyPrefsNotCleared();
  Mock::VerifyAndClearExpectations(&wrapper_);

  DestroyWrapper();
  VerifyPrefsNotCleared();
}

// Populate the prefs holding the lists of apps for which push-install has been
// requested and is still pending. Disable reporting. Create a wrapper. Verify
// that no reporter is created and the prefs are cleared.
TEST_F(AppInstallEventLogManagerWrapperTest, DisableCreate) {
  PopulatePrefs();
  profile_.GetPrefs()->SetBoolean(ash::prefs::kArcAppInstallEventLoggingEnabled,
                                  false);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateEncryptedReporter()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyEncryptedReporter());
  InitWrapper();
  VerifyPrefsCleared();
}

// Disable reporting. Create a wrapper. Verify that no reporter is created.
// Then, enable reporting. Verify that a reporter is created. Populate the prefs
// holding the lists of apps for which push-install has been requested and is
// still pending. Then, destroy the wrapper. Verify that the prefs are not
// cleared.
TEST_F(AppInstallEventLogManagerWrapperTest, CreateEnable) {
  profile_.GetPrefs()->SetBoolean(ash::prefs::kArcAppInstallEventLoggingEnabled,
                                  false);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateEncryptedReporter()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyEncryptedReporter());
  InitWrapper();
  Mock::VerifyAndClearExpectations(&wrapper_);

  EXPECT_CALL(*wrapper_, CreateEncryptedReporter());
  EXPECT_CALL(*wrapper_, DestroyEncryptedReporter()).Times(0);
  profile_.GetPrefs()->SetBoolean(ash::prefs::kArcAppInstallEventLoggingEnabled,
                                  true);
  Mock::VerifyAndClearExpectations(&wrapper_);

  PopulatePrefs();

  DestroyWrapper();
  VerifyPrefsNotCleared();
}

// Populate the prefs holding the lists of apps for which push-install has been
// requested and is still pending. Enable reporting. Create a wrapper. Verify
// that a reporter is created and the prefs are not cleared. Then, disable
// reporting. Verify that the reporter is destroyed and the prefs are cleared.
TEST_F(AppInstallEventLogManagerWrapperTest, CreateDisable) {
  PopulatePrefs();
  profile_.GetPrefs()->SetBoolean(ash::prefs::kArcAppInstallEventLoggingEnabled,
                                  true);

  CreateWrapper();

  EXPECT_CALL(*wrapper_, CreateEncryptedReporter());
  EXPECT_CALL(*wrapper_, DestroyEncryptedReporter()).Times(0);
  InitWrapper();
  VerifyPrefsNotCleared();
  Mock::VerifyAndClearExpectations(&wrapper_);

  EXPECT_CALL(*wrapper_, CreateEncryptedReporter()).Times(0);
  EXPECT_CALL(*wrapper_, DestroyEncryptedReporter());
  profile_.GetPrefs()->SetBoolean(ash::prefs::kArcAppInstallEventLoggingEnabled,
                                  false);
  Mock::VerifyAndClearExpectations(&wrapper_);
  VerifyPrefsCleared();
}

}  // namespace policy
