// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/arc_data_snapshotd_delegate.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace data_snapshotd {

namespace {

// This class is observer of ArcSessionManager. It makes sure that all ARC
// started/stopped events are tracked.
class TestObserver : public arc::ArcSessionManagerObserver {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  // arc::ArcSessionManagerObserver overrides:
  void OnArcStarted() override {
    EXPECT_FALSE(start_closure_.is_null());
    std::move(start_closure_).Run();
  }
  void OnArcSessionStopped(arc::ArcStopReason reason) override {
    EXPECT_FALSE(stop_closure_.is_null());
    std::move(stop_closure_).Run();
  }

  void set_start_closure(base::OnceClosure closure) {
    start_closure_ = std::move(closure);
  }
  void set_stop_closure(base::OnceClosure closure) {
    stop_closure_ = std::move(closure);
  }

 private:
  base::OnceClosure start_closure_;
  base::OnceClosure stop_closure_;
};

}  // namespace

class ArcDataSnapshotdDelegateTest : public testing::Test {
 public:
  ArcDataSnapshotdDelegateTest()
      : user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()) {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::EnableCheckAndroidManagementForTesting(false);

    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::SessionManagerClient::InitializeFakeInMemory();

    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    testing_profile_ = profile_builder.Build();

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        testing_profile_->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

  ArcDataSnapshotdDelegateTest(const ArcDataSnapshotdDelegateTest&) = delete;
  ArcDataSnapshotdDelegateTest& operator=(const ArcDataSnapshotdDelegateTest&) =
      delete;

  ~ArcDataSnapshotdDelegateTest() override {
    ash::SessionManagerClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

  void SetUp() override {
    arc_session_manager_ =
        arc::CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    arc_session_manager_->SetProfile(testing_profile_.get());

    PrefService* const prefs = testing_profile_->GetPrefs();
    prefs->SetBoolean(prefs::kArcEnabled, true);

    arc_session_manager_->Initialize();
    arc_session_manager_->AddObserver(&test_observer_);

    delegate_ = std::make_unique<ArcDataSnapshotdDelegate>();
  }

  void TearDown() override {
    delegate_.reset();
    arc_session_manager_->RemoveObserver(&test_observer_);
    arc_session_manager_.reset();
  }

  void EnableArc() {
    base::RunLoop run_loop;
    test_observer_.set_start_closure(run_loop.QuitClosure());
    arc_session_manager_->StartArcForTesting();
    run_loop.Run();
    EXPECT_TRUE(
        IsArcPlayStoreEnabledForProfile(arc_session_manager_->profile()));
  }

  void DisableArcPref() {
    PrefService* const prefs = testing_profile_->GetPrefs();
    prefs->SetBoolean(prefs::kArcEnabled, false);
    EXPECT_FALSE(
        IsArcPlayStoreEnabledForProfile(arc_session_manager_->profile()));
  }

  void ExpectArcStopped(base::OnceClosure closure) {
    test_observer_.set_stop_closure(std::move(closure));
  }

  ArcDataSnapshotdDelegate* delegate() { return delegate_.get(); }

 private:
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestObserver test_observer_;
  user_manager::ScopedUserManager user_manager_enabler_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ArcDataSnapshotdDelegate> delegate_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
};

// Tests ARC was stopped before the request.
TEST_F(ArcDataSnapshotdDelegateTest, BasicArcPreStopped) {
  EXPECT_EQ(arc::ArcSessionManager::State::STOPPED,
            arc::ArcSessionManager::Get()->state());
  base::RunLoop run_loop;
  delegate()->RequestStopArcInstance(
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Tests ARC is running and then stopped scenario.
TEST_F(ArcDataSnapshotdDelegateTest, BasicArcActive) {
  EnableArc();
  EXPECT_EQ(arc::ArcSessionManager::State::ACTIVE,
            arc::ArcSessionManager::Get()->state());

  base::RunLoop run_loop;
  ExpectArcStopped(run_loop.QuitClosure());
  delegate()->RequestStopArcInstance(
      base::BindLambdaForTesting([](bool success) { EXPECT_TRUE(success); }));
  run_loop.Run();
}

// Tests ARC is running and RequestStopArcInstance is called twice before ARC is
// stopped.
// RequestDisable() is a sync way of stopping ARC, need to interfere from
// test_observer and re-enable ARC to fake this scenario.
TEST_F(ArcDataSnapshotdDelegateTest, DoubleArcStop) {
  EnableArc();
  EXPECT_EQ(arc::ArcSessionManager::State::ACTIVE,
            arc::ArcSessionManager::Get()->state());

  base::RunLoop run_loop;
  // Expect ARC to be stopped for the first time.
  ExpectArcStopped(base::BindLambdaForTesting([&]() {
    // Request ARC to be stopped for the second time.
    delegate()->RequestStopArcInstance(
        base::BindLambdaForTesting([&run_loop](bool success) {
          // ARC is requested to be stopped for the second time. It is
          // an incorrect scenario, both callbacks return failures.
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
  }));
  // Request ARC to be stooped the first time.
  delegate()->RequestStopArcInstance(
      base::BindLambdaForTesting([](bool success) {
        // ARC is requested to be stopped twice. It is an incorrect scenario,
        // both callbacks return failures.
        EXPECT_FALSE(success);
      }));
  run_loop.Run();
}

}  // namespace data_snapshotd
}  // namespace arc
