// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class ArcBootPhaseMonitorBridgeTest : public testing::Test {
 public:
  ArcBootPhaseMonitorBridgeTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::SessionManagerClient::InitializeFakeInMemory();

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        testing_profile_->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    boot_phase_monitor_bridge_ =
        ArcBootPhaseMonitorBridge::GetForBrowserContextForTesting(
            testing_profile_.get());
    boot_phase_monitor_bridge_->SetDelegateForTesting(
        std::make_unique<TestDelegateImpl>(this));
  }

  ArcBootPhaseMonitorBridgeTest(const ArcBootPhaseMonitorBridgeTest&) = delete;
  ArcBootPhaseMonitorBridgeTest& operator=(
      const ArcBootPhaseMonitorBridgeTest&) = delete;

  ~ArcBootPhaseMonitorBridgeTest() override {
    boot_phase_monitor_bridge_->Shutdown();
    testing_profile_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    ash::SessionManagerClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

 protected:
  class TestObserverImpl : public ArcBootPhaseMonitorBridge::Observer {
   public:
    explicit TestObserverImpl(ArcBootPhaseMonitorBridgeTest* test)
        : test_(test) {}
    TestObserverImpl(const TestObserverImpl&) = delete;
    TestObserverImpl& operator=(const TestObserverImpl&) = delete;
    ~TestObserverImpl() override = default;

    void OnBootCompleted() override { ++(test_->on_boot_completed_counter_); }

   private:
    ArcBootPhaseMonitorBridgeTest* const test_;
  };

  ArcSessionManager* arc_session_manager() const {
    return arc_session_manager_.get();
  }
  ArcBootPhaseMonitorBridge* boot_phase_monitor_bridge() const {
    return boot_phase_monitor_bridge_;
  }
  size_t record_uma_counter() const { return record_uma_counter_; }
  base::TimeDelta last_time_delta() const { return last_time_delta_; }
  size_t on_boot_completed_counter() const {
    return on_boot_completed_counter_;
  }

  sync_preferences::TestingPrefServiceSyncable* GetPrefs() const {
    return testing_profile_->GetTestingPrefService();
  }

 private:
  class TestDelegateImpl : public ArcBootPhaseMonitorBridge::Delegate {
   public:
    explicit TestDelegateImpl(ArcBootPhaseMonitorBridgeTest* test)
        : test_(test) {}
    TestDelegateImpl(const TestDelegateImpl&) = delete;
    TestDelegateImpl& operator=(const TestDelegateImpl&) = delete;
    ~TestDelegateImpl() override = default;

    void RecordFirstAppLaunchDelayUMA(base::TimeDelta delta) override {
      test_->last_time_delta_ = delta;
      ++(test_->record_uma_counter_);
    }

   private:
    ArcBootPhaseMonitorBridgeTest* const test_;
  };

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  ArcBootPhaseMonitorBridge* boot_phase_monitor_bridge_;

  size_t record_uma_counter_ = 0;
  base::TimeDelta last_time_delta_;
  size_t on_boot_completed_counter_ = 0;
};

// Tests that ArcBootPhaseMonitorBridge can be constructed and destructed.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestConstructDestruct) {}

// Tests that ArcBootPhaseMonitorBridge::Observer is called.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestObserver) {
  TestObserverImpl observer(this);
  boot_phase_monitor_bridge()->AddObserver(&observer);
  EXPECT_EQ(0u, on_boot_completed_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(1u, on_boot_completed_counter());
  boot_phase_monitor_bridge()->RemoveObserver(&observer);
}

// Tests that ArcBootPhaseMonitorBridge::Observer is called even when it is
// added after ARC is fully started.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestObserver_DelayedAdd) {
  TestObserverImpl observer(this);
  EXPECT_EQ(0u, on_boot_completed_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(0u, on_boot_completed_counter());
  boot_phase_monitor_bridge()->AddObserver(&observer);
  EXPECT_EQ(1u, on_boot_completed_counter());
  boot_phase_monitor_bridge()->RemoveObserver(&observer);
}

// Tests that the UMA recording function is never called unless
// RecordFirstAppLaunchDelayUMA is called.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestRecordUMA_None) {
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnArcSessionStopped(ArcStopReason::SHUTDOWN);
  EXPECT_EQ(0U, record_uma_counter());
}

// Tests that RecordFirstAppLaunchDelayUMA() actually calls the UMA recording
// function (but only after OnBootCompleted.)
TEST_F(ArcBootPhaseMonitorBridgeTest, TestRecordUMA_AppLaunchBeforeBoot) {
  EXPECT_EQ(0U, record_uma_counter());
  // Calling RecordFirstAppLaunchDelayUMA() before boot shouldn't immediately
  // record UMA.
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(0U, record_uma_counter());
  // Sleep for 1ms just to make sure 0 won't be passed to RecordUMA().
  base::PlatformThread::Sleep(base::Milliseconds(1));
  // UMA recording should be done on BootCompleted.
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(1U, record_uma_counter());
  // In this case, |delta| passed to the UMA recording function should be >0.
  EXPECT_LT(base::TimeDelta(), last_time_delta());
}

// Tests the same with calling RecordFirstAppLaunchDelayUMA() after boot.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestRecordUMA_AppLaunchAfterBoot) {
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(0U, record_uma_counter());
  // Calling RecordFirstAppLaunchDelayUMA() after boot should immediately record
  // UMA.
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(1U, record_uma_counter());
  // In this case, |delta| passed to the UMA recording function should be 0.
  EXPECT_TRUE(last_time_delta().is_zero());
}

// Tests the same with calling RecordFirstAppLaunchDelayUMA() twice.
TEST_F(ArcBootPhaseMonitorBridgeTest,
       TestRecordUMA_AppLaunchesBeforeAndAfterBoot) {
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(1U, record_uma_counter());
  EXPECT_LT(base::TimeDelta(), last_time_delta());
  // Call the record function again and check that the counter is not changed.
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(1U, record_uma_counter());
}

// Tests the same with calling RecordFirstAppLaunchDelayUMA() twice after boot.
TEST_F(ArcBootPhaseMonitorBridgeTest, TestRecordUMA_AppLaunchesAfterBoot) {
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->OnBootCompleted();
  EXPECT_EQ(0U, record_uma_counter());
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(1U, record_uma_counter());
  EXPECT_TRUE(last_time_delta().is_zero());
  // Call the record function again and check that the counter is not changed.
  boot_phase_monitor_bridge()->RecordFirstAppLaunchDelayUMAForTesting();
  EXPECT_EQ(1U, record_uma_counter());
}
}  // namespace

}  // namespace arc
