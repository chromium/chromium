// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_boot_phase_throttle_observer.h"

#include <memory>

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcBootPhaseThrottleObserverTest : public testing::Test {
 public:
  ArcBootPhaseThrottleObserverTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    // Need to initialize DBusThreadManager before ArcSessionManager's
    // constructor calls DBusThreadManager::Get().
    chromeos::DBusThreadManager::Initialize();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    // Setup and login profile
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        testing_profile_->GetProfileUserName(), ""));
    auto* user_manager = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
    user_manager->AddUser(account_id);
    user_manager->LoginUser(account_id);

    // By default, ARC is not started for opt-in.
    arc_session_manager()->set_directly_started_for_testing(true);

    ArcBootPhaseMonitorBridge::GetForBrowserContextForTesting(
        testing_profile_.get());
    observer()->StartObserving(
        testing_profile_.get(),
        ArcBootPhaseThrottleObserver::ObserverStateChangedCallback());
  }

  void TearDown() override {
    observer()->StopObserving();
    testing_profile_.reset();
    arc_session_manager_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefs() {
    return testing_profile_->GetTestingPrefService();
  }

  ArcBootPhaseThrottleObserver* observer() { return &observer_; }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  ArcServiceManager arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  ArcBootPhaseThrottleObserver observer_;
  std::unique_ptr<TestingProfile> testing_profile_;

  DISALLOW_COPY_AND_ASSIGN(ArcBootPhaseThrottleObserverTest);
};

TEST_F(ArcBootPhaseThrottleObserverTest, TestConstructDestruct) {}

// Lock is enabled during boot when session restore is not occurring
TEST_F(ArcBootPhaseThrottleObserverTest, TestOnArcStarted) {
  EXPECT_FALSE(observer()->active());

  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  observer()->OnBootCompleted();
  EXPECT_FALSE(observer()->active());

  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  observer()->OnArcInitialStart();
  EXPECT_FALSE(observer()->active());
}

// Lock is disabled during session restore, and re-enabled if ARC
// is still booting
TEST_F(ArcBootPhaseThrottleObserverTest, TestSessionRestore) {
  EXPECT_FALSE(observer()->active());
  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  observer()->OnSessionRestoreStartedLoadingTabs();
  EXPECT_FALSE(observer()->active());
  observer()->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_TRUE(observer()->active());
  observer()->OnBootCompleted();
  EXPECT_FALSE(observer()->active());
}

// Lock is enabled during ARC restart until boot is completed.
TEST_F(ArcBootPhaseThrottleObserverTest, TestOnArcRestart) {
  EXPECT_FALSE(observer()->active());
  observer()->OnArcSessionRestarting();
  EXPECT_TRUE(observer()->active());
  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  observer()->OnBootCompleted();
  EXPECT_FALSE(observer()->active());
}

// Lock is enabled during session restore because ARC is started by enterprise
// policy.
TEST_F(ArcBootPhaseThrottleObserverTest, TestEnabledByEnterprise) {
  EXPECT_FALSE(observer()->active());
  GetPrefs()->SetManagedPref(prefs::kArcEnabled,
                             std::make_unique<base::Value>(true));
  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  observer()->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(observer()->active());
  observer()->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_TRUE(observer()->active());
  observer()->OnBootCompleted();
  EXPECT_FALSE(observer()->active());
}

// Lock is enabled during session restore because ARC was started for opt-in.
TEST_F(ArcBootPhaseThrottleObserverTest, TestOptInBoot) {
  EXPECT_FALSE(observer()->active());
  arc_session_manager()->set_directly_started_for_testing(false);
  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  observer()->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(observer()->active());
  observer()->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_TRUE(observer()->active());
  observer()->OnBootCompleted();
  EXPECT_FALSE(observer()->active());
}

}  // namespace arc
