// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scheduled_restart_manager.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/scheduled_restart_test_utils.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/idle/idle_polling_service.h"
#include "ui/base/idle/idle_time_provider.h"
#include "ui/base/test/idle_test_utils.h"

namespace scheduled_restart {

namespace {

class TestIdleTimeProvider : public ui::IdleTimeProvider {
 public:
  base::TimeDelta CalculateIdleTime() override { return base::Seconds(0); }
  bool CheckIdleStateIsLocked() override { return false; }
};

}  // namespace

class ScheduledRestartManagerBrowserTest : public InProcessBrowserTest {
 public:
  ScheduledRestartManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kScheduledRestart, {{"idle_threshold", "300s"}});
  }
  ~ScheduledRestartManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    UserEducationServiceFactory::GetInstance()
        ->disable_idle_polling_for_testing();
    fake_upgrade_detector_ = std::make_unique<FakeUpgradeDetector>();
    if (auto* instance =
            g_browser_process->GetFeatures()->scheduled_restart_manager()) {
      instance->CancelSchedule();
    }

    scoped_idle_provider_ =
        std::make_unique<ui::test::ScopedIdleProviderForTest>(
            std::make_unique<TestIdleTimeProvider>());
  }

  void TearDownOnMainThread() override {
    if (auto* instance =
            g_browser_process->GetFeatures()->scheduled_restart_manager()) {
      instance->CancelSchedule();
    }
    fake_upgrade_detector_.reset();
    scoped_idle_provider_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeUpgradeDetector> fake_upgrade_detector_;
  std::unique_ptr<ui::test::ScopedIdleProviderForTest> scoped_idle_provider_;
};

IN_PROC_BROWSER_TEST_F(ScheduledRestartManagerBrowserTest,
                       GlobalInstanceCreationAndHelpers) {
  ScheduledRestartManager* instance =
      g_browser_process->GetFeatures()->scheduled_restart_manager();
  ASSERT_NE(nullptr, instance);

  EXPECT_FALSE(instance->is_scheduled());
  EXPECT_EQ(ScheduledRestartMode::kNone, instance->mode());

  instance->ScheduleRestartOnIdle();
  EXPECT_TRUE(instance->is_scheduled());
  EXPECT_EQ(ScheduledRestartMode::kOnIdle, instance->mode());

  instance->CancelSchedule();
  EXPECT_FALSE(instance->is_scheduled());
  EXPECT_EQ(ScheduledRestartMode::kNone, instance->mode());
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartManagerBrowserTest, IdleRelaunch) {
  // 1. Chrome starts without an upgrade detected.
  ScheduledRestartManager manager(*fake_upgrade_detector_);
  EXPECT_FALSE(manager.is_scheduled());

  // 2. An upgrade arrives later, triggering OnUpgradeRecommended.
  fake_upgrade_detector_->SetUpgradeAvailable();

  // 3. User schedules restart on idle.
  manager.ScheduleRestartOnIdle();
  EXPECT_TRUE(manager.is_scheduled());

  bool relaunch_called = false;
  manager.set_relaunch_callback_for_testing(
      base::BindLambdaForTesting([&]() { relaunch_called = true; }));

  base::TimeDelta threshold = ScheduledRestartManager::GetIdleThreshold();

  // 4. Idle time under threshold -> no relaunch, schedule preserved.
  ui::IdlePollingService::State idle_state;
  idle_state.idle_time = threshold - base::Seconds(1);
  manager.OnIdleStateChange(idle_state);
  EXPECT_FALSE(relaunch_called);
  EXPECT_TRUE(manager.is_scheduled());

  // 5. Upgrade annoyance level escalates, firing OnUpgradeRecommended again.
  fake_upgrade_detector_->NotifyUpgrade();
  EXPECT_TRUE(manager.is_scheduled());

  // 6. Idle time reaches threshold -> relaunch triggered and schedule reset.
  idle_state.idle_time = threshold + base::Seconds(1);
  manager.OnIdleStateChange(idle_state);
  EXPECT_TRUE(relaunch_called);
  EXPECT_FALSE(manager.is_scheduled());
}

IN_PROC_BROWSER_TEST_F(ScheduledRestartManagerBrowserTest,
                       ProtectsAgainstMultipleRestarts) {
  fake_upgrade_detector_->SetUpgradeAvailable();

  ScheduledRestartManager manager(*fake_upgrade_detector_);

  int relaunch_count = 0;
  manager.set_relaunch_callback_for_testing(
      base::BindLambdaForTesting([&]() { ++relaunch_count; }));

  manager.ScheduleRestartOnIdle();

  base::TimeDelta threshold = ScheduledRestartManager::GetIdleThreshold();

  ui::IdlePollingService::State idle_state;
  idle_state.idle_time = threshold + base::Seconds(1);
  manager.OnIdleStateChange(idle_state);
  EXPECT_EQ(1, relaunch_count);

  // Subsequent call while executing restart does not trigger callback again.
  manager.OnIdleStateChange(idle_state);
  EXPECT_EQ(1, relaunch_count);
}

}  // namespace scheduled_restart
