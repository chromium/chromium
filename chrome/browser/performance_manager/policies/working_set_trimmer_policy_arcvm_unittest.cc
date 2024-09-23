// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_arcvm.h"

#include <memory>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_host.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/command_line.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/display/test/test_screen.h"
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
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();

    app_host_ = std::make_unique<arc::FakeAppHost>(
        arc_service_manager_->arc_bridge_service()->app());
    app_instance_ = std::make_unique<arc::FakeAppInstance>(app_host_.get());
    intent_helper_host_ = std::make_unique<arc::FakeIntentHelperHost>(
        arc_service_manager_->arc_bridge_service()->intent_helper());
    intent_helper_instance_ = std::make_unique<arc::FakeIntentHelperInstance>();

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
    intent_helper_instance_.reset();
    intent_helper_host_.reset();
    app_instance_.reset();
    app_host_.reset();
    arc_service_manager_.reset();

    // All other object must be destroyed before shutting down ConciergeClient.
    ash::ConciergeClient::Shutdown();
    arc::StabilityMetricsManager::Shutdown();
  }

  WorkingSetTrimmerPolicyArcVmTest(const WorkingSetTrimmerPolicyArcVmTest&) =
      delete;
  WorkingSetTrimmerPolicyArcVmTest& operator=(
      const WorkingSetTrimmerPolicyArcVmTest&) = delete;

  void ConnectAppMojo() {
    arc_service_manager_->arc_bridge_service()->app()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_service_manager_->arc_bridge_service()->app());
  }

  void ConnectIntentHelperMojo() {
    arc_service_manager_->arc_bridge_service()->intent_helper()->SetInstance(
        intent_helper_instance_.get());
    WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->intent_helper());
  }

  void ConnectMojoToCallOnConnectionReady() {
    ConnectAppMojo();
    ConnectIntentHelperMojo();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  base::TimeDelta GetInterval() { return base::Minutes(1); }
  WorkingSetTrimmerPolicyArcVm* trimmer() { return policy_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};
  TestingPrefServiceSimple local_state_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::FakeAppHost> app_host_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  std::unique_ptr<arc::FakeIntentHelperHost> intent_helper_host_;
  std::unique_ptr<arc::FakeIntentHelperInstance> intent_helper_instance_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<WorkingSetTrimmerPolicyArcVm> policy_;
};

// Tests that IsEligibleForReclaim() returns kReclaimNone initially.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, InitialState) {
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimNone,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
}

// Tests that IsEligibleForReclaim() returns kReclaimNone right after boot
// completion but kReclaimAll after the period.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, BootComplete) {
  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimNone,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));

  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimAll,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
}

// Tests that IsEligibleForReclaim() always returns kReclaimNone if the mojo
// connection is terminated after boot.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, BootCompleteThenDisconnect) {
  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(base::Seconds(1));

  // Disconnect mojo.
  trimmer()->OnArcSessionStopped(arc::ArcStopReason::CRASH);
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  // Even if IsEligibleForReclaim() is called with kReclaimAll, it returns
  // kReclaimNone because the mojo connection is gone.
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimNone,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimAll, nullptr));
}

// Tests the same but with OnArcSessionRestarting().
TEST_F(WorkingSetTrimmerPolicyArcVmTest, BootCompleteThenDisconnect2) {
  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(base::Seconds(1));

  // Disconnect mojo.
  trimmer()->OnArcSessionRestarting();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  // Even if IsEligibleForReclaim() is called with kReclaimAll, it returns
  // kReclaimNone because the mojo connection is gone.
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimNone,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimAll, nullptr));
}

// Tests that IsEligibleForReclaim() returns kReclaimNone right after user
// interaction.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, UserInteraction) {
  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimAll,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
  trimmer()->OnUserInteraction(
      arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER);
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimNone,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimAll,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
}

// Tests that IsEligibleForReclaim() returns kReclaimNone when ARCVM is no
// longer running.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, ArcVmNotRunning) {
  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimAll,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
  trimmer()->OnArcSessionRestarting();
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimNone,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));

  trimmer()->OnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimAll,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
  trimmer()->OnArcSessionStopped(arc::ArcStopReason::CRASH);
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimNone,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
}

// Tests that IsEligibleForReclaim() returns kReclaimNone when ARCVM is focused.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, WindowFocused) {
  // Create container window as the parent for other windows.
  aura::Window container_window(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  container_window.Init(ui::LAYER_NOT_DRAWN);

  // Create two fake windows.
  aura::Window* arc_window = aura::test::CreateTestWindow(
      SK_ColorGREEN, 0, gfx::Rect(), &container_window);
  arc_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  ASSERT_TRUE(ash::IsArcWindow(arc_window));
  aura::Window* chrome_window = aura::test::CreateTestWindow(
      SK_ColorRED, 0, gfx::Rect(), &container_window);
  ASSERT_FALSE(ash::IsArcWindow(chrome_window));

  bool is_first_trim_post_boot = true;

  // Initially, Chrome window is focused.
  trimmer()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      chrome_window, nullptr);

  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  // After boot, ARCVM becomes eligible to reclaim.
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimAll,
            trimmer()->IsEligibleForReclaim(
                GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone,
                &is_first_trim_post_boot));

  // Although this is the first attempt post-boot, it will not be flagged
  // as such because we pass kReclaimNone above, which disables the
  // first-boot-after-trim feature and makes the flag irrelevant.
  EXPECT_EQ(is_first_trim_post_boot, false);

  // ARCVM window is focused. ARCVM is ineligible to reclaim now.
  trimmer()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT, arc_window,
      chrome_window);
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone,
                &is_first_trim_post_boot));
  EXPECT_EQ(is_first_trim_post_boot, false);
  is_first_trim_post_boot = true;  // So we can check that it flips to false.
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone,
                &is_first_trim_post_boot));
  EXPECT_EQ(is_first_trim_post_boot, false);
  is_first_trim_post_boot = true;  // So we can check that it flips to false.

  // ARCVM window is unfocused. ARCVM becomes eligible to reclaim after the
  // period.
  trimmer()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT,
      chrome_window, arc_window);
  EXPECT_EQ(
      mechanism::ArcVmReclaimType::kReclaimNone,
      trimmer()->IsEligibleForReclaim(
          GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone, nullptr));
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimAll,
            trimmer()->IsEligibleForReclaim(
                GetInterval(), mechanism::ArcVmReclaimType::kReclaimNone,
                &is_first_trim_post_boot));
  EXPECT_EQ(is_first_trim_post_boot, false);
}

// Tests that IsEligibleForReclaim(.., kReclaimAll) returns kReclaimAll right
// after boot completion.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, TrimOnBootCompleteWithReclaimAll) {
  bool is_first_trim_post_boot = true;
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(), mechanism::ArcVmReclaimType::kReclaimAll,
                &is_first_trim_post_boot));
  // It's not time yet.
  EXPECT_EQ(is_first_trim_post_boot, false);

  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  // IsEligibleForReclaim() returns kReclaimAll after boot completion.
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimAll,
            trimmer()->IsEligibleForReclaim(
                GetInterval(), mechanism::ArcVmReclaimType::kReclaimAll,
                &is_first_trim_post_boot));
  EXPECT_EQ(is_first_trim_post_boot, true);
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(), mechanism::ArcVmReclaimType::kReclaimAll,
                &is_first_trim_post_boot));
  EXPECT_EQ(is_first_trim_post_boot, false);  // It's no longer "first boot".

  is_first_trim_post_boot = true;  // So we can check that it flips below.
  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  // After the interval, the function returns kReclaimAll again.
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimAll,
            trimmer()->IsEligibleForReclaim(
                GetInterval(), mechanism::ArcVmReclaimType::kReclaimAll,
                &is_first_trim_post_boot));
  EXPECT_EQ(is_first_trim_post_boot, false);
}

// Tests that IsEligibleForReclaim(.., kReclaimGuestPageCaches) returns
// kReclaimGuestPageCaches right after boot completion.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, TrimOnBootComplete) {
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));

  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  // IsEligibleForReclaim() returns kReclaimGuestPageCaches after boot
  // completion (but only once.)
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));

  FastForwardBy(GetInterval());
  FastForwardBy(base::Seconds(1));
  // After the interval, the function returns kReclaimAll again.
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimAll,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
}

// Tests that IsEligibleForReclaim(.., true) returns kReclaimGuestPageCaches for
// each ARCVM boot.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, TrimOnBootCompleteAfterArcVmRestart) {
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));

  // Indirectly call OnConnectionReady and OnConnectionReadyInternal.
  ConnectMojoToCallOnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));

  trimmer()->OnArcSessionRestarting();
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
  trimmer()->OnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  // After ARCVM restart, the functions returns kReclaimGuestPageCaches again.
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));

  // Tests the same with OnArcSessionStopped().
  trimmer()->OnArcSessionStopped(arc::ArcStopReason::CRASH);
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
  trimmer()->OnConnectionReady();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());

  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
}

// Tests that IsEligibleForReclaim(.., kReclaimGuestPageCaches) doesn't return
// kReclaimGuestPageCaches until both mojo connections are established.
TEST_F(WorkingSetTrimmerPolicyArcVmTest, MojoConnection) {
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));

  ConnectAppMojo();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting());
  // Since intent_helper.mojom is not connected yet, it returns kReclaimNone.
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));

  ConnectIntentHelperMojo();
  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting() /
                2);
  // Since |kArcVmBootDelay| hasn't elapsed yet, it returns kReclaimNone.
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimNone,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));

  FastForwardBy(WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting() /
                2);
  EXPECT_EQ(mechanism::ArcVmReclaimType::kReclaimGuestPageCaches,
            trimmer()->IsEligibleForReclaim(
                GetInterval(),
                mechanism::ArcVmReclaimType::kReclaimGuestPageCaches, nullptr));
}

}  // namespace
}  // namespace policies
}  // namespace performance_manager
