// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_arcvm.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/constants/app_types.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/command_line.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/gfx/geometry/rect.h"

namespace performance_manager {
namespace policies {
namespace {

class WorkingSetTrimmerPolicyArcVmTest : public testing::Test {
 protected:
  WorkingSetTrimmerPolicyArcVmTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    base::CommandLine::ForCurrentProcess()->InitFromArgv(
        {"", "--enable-arcvm"});
    arc::prefs::RegisterLocalStatePrefs(local_state_.registry());
    arc::StabilityMetricsManager::Initialize(&local_state_);
    chromeos::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();
    policy_ =
        WorkingSetTrimmerPolicyArcVm::CreateForTesting(testing_profile_.get());

    arc::ArcBootPhaseMonitorBridge::GetForBrowserContextForTesting(
        testing_profile_.get());
    arc::ArcMetricsService::GetForBrowserContextForTesting(
        testing_profile_.get());

    // Set the state to "logged in".
    session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  }

  ~WorkingSetTrimmerPolicyArcVmTest() override {
    policy_.reset();
    testing_profile_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();

    // All other object must be destroyed before shutting down ConciergeClient.
    chromeos::ConciergeClient::Shutdown();
    arc::StabilityMetricsManager::Shutdown();
  }

  WorkingSetTrimmerPolicyArcVmTest(const WorkingSetTrimmerPolicyArcVmTest&) =
      delete;
  WorkingSetTrimmerPolicyArcVmTest& operator=(
      const WorkingSetTrimmerPolicyArcVmTest&) = delete;

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  base::TimeDelta GetInterval() { return base::Minutes(1); }
  WorkingSetTrimmerPolicyArcVm* trimmer() { return policy_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<WorkingSetTrimmerPolicyArcVm> policy_;
};

// Tests that IsEligibleForReclaim() returns false initially.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, InitialState) {
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
}

// Tests that IsEligibleForReclaim() returns false right after boot completion
// but true after the period.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, BootComplete) {
  trimmer()->OnConnectionReady();
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
}

// Tests that IsEligibleForReclaim() returns false right after user
// interaction.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, UserInteraction) {
  trimmer()->OnConnectionReady();
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
  trimmer()->OnUserInteraction(
      arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER);
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
}

// Tests that IsEligibleForReclaim() returns false when ARCVM is no longer
// running.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, ArcVmNotRunning) {
  trimmer()->OnConnectionReady();
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
  trimmer()->OnArcSessionRestarting();
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), false));

  trimmer()->OnConnectionReady();
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
  trimmer()->OnArcSessionStopped(arc::ArcStopReason::CRASH);
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
}

// Tests that IsEligibleForReclaim() returns false when ARCVM is focused.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, WindowFocused) {
  // Create container window as the parent for other windows.
  aura::Window container_window(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  container_window.Init(ui::LAYER_NOT_DRAWN);

  // Create two fake windows.
  aura::Window* arc_window = aura::test::CreateTestWindow(
      SK_ColorGREEN, 0, gfx::Rect(), &container_window);
  arc_window->SetProperty(aura::client::kAppType,
                          static_cast<int>(ash::AppType::ARC_APP));
  ASSERT_TRUE(ash::IsArcWindow(arc_window));
  aura::Window* chrome_window = aura::test::CreateTestWindow(
      SK_ColorRED, 0, gfx::Rect(), &container_window);
  ASSERT_FALSE(ash::IsArcWindow(chrome_window));

  // Initially, Chrome window is focused.
  trimmer()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      chrome_window, nullptr);

  // After boot, ARCVM becomes eligible to reclaim.
  trimmer()->OnConnectionReady();
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), false));

  // ARCVM window is focused. ARCVM is ineligible to reclaim now.
  trimmer()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT, arc_window,
      chrome_window);
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), false));

  // ARCVM window is unfocused. ARCVM becomes eligible to reclaim after the
  // period.
  trimmer()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      chrome_window, arc_window);
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), false));
}

// Tests that IsEligibleForReclaim(.., true) returns true right after boot
// completion.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, TrimOnBootComplete) {
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
  trimmer()->OnConnectionReady();
  // IsEligibleForReclaim() returns true after boot completion (but only once.)
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), true));

  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  // After the interval, the function returns true again.
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
}

// Tests that IsEligibleForReclaim(.., true) returns true for each ARCVM boot.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, TrimOnBootCompleteAfterArcVmRestart) {
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
  trimmer()->OnConnectionReady();
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), true));

  trimmer()->OnArcSessionRestarting();
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
  trimmer()->OnConnectionReady();
  // After ARCVM restart, the functions returns true again.
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), true));

  // Tests the same with OnArcSessionStopped().
  trimmer()->OnArcSessionStopped(arc::ArcStopReason::CRASH);
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
  trimmer()->OnConnectionReady();
  EXPECT_TRUE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
  EXPECT_FALSE(trimmer()->IsEligibleForReclaim(GetInterval(), true));
}

}  // namespace
}  // namespace policies
}  // namespace performance_manager
