// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_boot_phase_throttle_observer.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_host.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcBootPhaseThrottleObserverTest : public testing::Test {
 public:
  ArcBootPhaseThrottleObserverTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    // Setup and login profile
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        testing_profile_->GetProfileUserName(), ""));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    // By default, ARC is not started for opt-in.
    arc_session_manager()->set_skipped_terms_of_service_negotiation_for_testing(
        true);

    observer()->StartObserving(
        testing_profile_.get(),
        ArcBootPhaseThrottleObserver::ObserverStateChangedCallback());

    app_host_ = std::make_unique<FakeAppHost>(
        arc_service_manager_.arc_bridge_service()->app());
    app_instance_ = std::make_unique<FakeAppInstance>(app_host_.get());
    intent_helper_host_ = std::make_unique<FakeIntentHelperHost>(
        arc_service_manager_.arc_bridge_service()->intent_helper());
    intent_helper_instance_ = std::make_unique<FakeIntentHelperInstance>();
  }

  ArcBootPhaseThrottleObserverTest(const ArcBootPhaseThrottleObserverTest&) =
      delete;
  ArcBootPhaseThrottleObserverTest& operator=(
      const ArcBootPhaseThrottleObserverTest&) = delete;

  void TearDown() override {
    observer()->StopObserving();
    testing_profile_.reset();
    arc_session_manager_.reset();
    ash::ConciergeClient::Shutdown();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefs() {
    return testing_profile_->GetTestingPrefService();
  }

  void ConnectAppMojo() {
    arc_service_manager_.arc_bridge_service()->app()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_service_manager_.arc_bridge_service()->app());
  }

  void DisconnectAppMojo() {
    arc_service_manager_.arc_bridge_service()->app()->CloseInstance(
        app_instance_.get());
    base::RunLoop().RunUntilIdle();
  }

  void ConnectIntentHelperMojo() {
    arc_service_manager_.arc_bridge_service()->intent_helper()->SetInstance(
        intent_helper_instance_.get());
    WaitForInstanceReady(
        arc_service_manager_.arc_bridge_service()->intent_helper());
  }

  void ConnectMojoToCallOnConnectionReady() {
    ConnectAppMojo();
    ConnectIntentHelperMojo();
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }
  ArcBootPhaseThrottleObserver* observer() { return &observer_; }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  ArcServiceManager arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  ArcBootPhaseThrottleObserver observer_;
  std::unique_ptr<TestingProfile> testing_profile_;

  std::unique_ptr<FakeAppHost> app_host_;
  std::unique_ptr<FakeAppInstance> app_instance_;
  std::unique_ptr<FakeIntentHelperHost> intent_helper_host_;
  std::unique_ptr<FakeIntentHelperInstance> intent_helper_instance_;
};

TEST_F(ArcBootPhaseThrottleObserverTest, TestConstructDestruct) {}

// Lock is enabled during boot when session restore is not occurring
TEST_F(ArcBootPhaseThrottleObserverTest, TestOnArcStarted) {
  EXPECT_FALSE(observer()->active());

  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  ConnectMojoToCallOnConnectionReady();
  EXPECT_TRUE(observer()->active());
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());
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
  ConnectMojoToCallOnConnectionReady();
  EXPECT_TRUE(observer()->active());
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());
  EXPECT_FALSE(observer()->active());
}

// Lock is enabled during ARC restart until boot is completed.
TEST_F(ArcBootPhaseThrottleObserverTest, TestOnArcRestart) {
  EXPECT_FALSE(observer()->active());
  observer()->OnArcSessionRestarting();
  EXPECT_TRUE(observer()->active());
  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  ConnectMojoToCallOnConnectionReady();
  EXPECT_TRUE(observer()->active());
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());
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
  ConnectMojoToCallOnConnectionReady();
  EXPECT_TRUE(observer()->active());
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());
  EXPECT_FALSE(observer()->active());
}

// Lock is enabled during session restore because ARC was started for opt-in.
TEST_F(ArcBootPhaseThrottleObserverTest, TestOptInBoot) {
  EXPECT_FALSE(observer()->active());
  arc_session_manager()->set_skipped_terms_of_service_negotiation_for_testing(
      false);
  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  observer()->OnSessionRestoreStartedLoadingTabs();
  EXPECT_TRUE(observer()->active());
  observer()->OnSessionRestoreFinishedLoadingTabs();
  EXPECT_TRUE(observer()->active());
  ConnectMojoToCallOnConnectionReady();
  EXPECT_TRUE(observer()->active());
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());
  EXPECT_FALSE(observer()->active());
}

TEST_F(ArcBootPhaseThrottleObserverTest, TestAppMojoNotReady) {
  EXPECT_FALSE(observer()->active());

  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  ConnectIntentHelperMojo();
  EXPECT_TRUE(observer()->active());
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());
  // Since app.mojom is not ready, ARC is still unthrottled.
  EXPECT_TRUE(observer()->active());
}

TEST_F(ArcBootPhaseThrottleObserverTest, TestIntentHelperMojoNotReady) {
  EXPECT_FALSE(observer()->active());

  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  ConnectAppMojo();
  EXPECT_TRUE(observer()->active());
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());
  // Since intent_helper.mojom is not ready, ARC is still unthrottled.
  EXPECT_TRUE(observer()->active());
}

TEST_F(ArcBootPhaseThrottleObserverTest, TestAppMojoDisconnection) {
  EXPECT_FALSE(observer()->active());

  observer()->OnArcStarted();
  EXPECT_TRUE(observer()->active());
  ConnectAppMojo();
  DisconnectAppMojo();
  ConnectIntentHelperMojo();
  EXPECT_TRUE(observer()->active());
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());
  // Since app.mojom is not ready, ARC is still unthrottled.
  EXPECT_TRUE(observer()->active());
}

}  // namespace
}  // namespace arc
