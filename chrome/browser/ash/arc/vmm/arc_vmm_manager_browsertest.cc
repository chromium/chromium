// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_service_manager.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"
#include "chrome/browser/ash/arc/vmm/arc_vmm_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcVmmManagerBrowserTest : public InProcessBrowserTest {
 public:
  ArcVmmManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {kVmmSwapPolicy, kVmmSwapKeyboardShortcut}, {});
  }
  ArcVmmManagerBrowserTest(const ArcVmmManagerBrowserTest&) = delete;
  ArcVmmManagerBrowserTest& operator=(const ArcVmmManagerBrowserTest&) = delete;

  // InProcessBrowserTest:
  ~ArcVmmManagerBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
    command_line->AppendSwitch(ash::switches::kEnableArcVm);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    base::RunLoop().RunUntilIdle();
    EnableArc();
  }

  void TearDownOnMainThread() override { DisableArc(); }

  // ArcVmmManagerBrowserTest:
  void EnableArc() {
    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);

    arc_app_list_prefs_ = ArcAppListPrefs::Get(browser()->profile());
    DCHECK(arc_app_list_prefs_);

    base::RunLoop run_loop;
    arc_app_list_prefs_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    fake_app_instance_ =
        std::make_unique<arc::FakeAppInstance>(arc_app_list_prefs_);
    arc_app_list_prefs_->app_connection_holder()->SetInstance(
        fake_app_instance_.get());
    WaitForInstanceReady(arc_app_list_prefs_->app_connection_holder());
  }

  void DisableArc() {
    arc_app_list_prefs_->app_connection_holder()->CloseInstance(
        fake_app_instance_.get());
    fake_app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
    arc_app_list_prefs_ = nullptr;
  }

  ArcVmmManager* vmm_manager() {
    return ArcVmmManager::GetForBrowserContext(browser()->profile());
  }

  bool arc_connected() { return vmm_manager()->arc_connected_; }

  SwapState last_requested_swap_state() {
    return vmm_manager()->latest_swap_state_;
  }
  ArcVmmSwapScheduler* swap_scheduler() {
    return vmm_manager()->scheduler_.get();
  }
  ArcVmmManager::AcceleratorTarget* shortcut_target() {
    return vmm_manager()->accelerator_.get();
  }
  ArcSystemStateObservation* observation() {
    return static_cast<ArcSystemStateObservation*>(
        swap_scheduler()->peace_duration_provider_.get());
  }

 protected:
  std::unique_ptr<FakeAppInstance> fake_app_instance_;

  raw_ptr<ArcAppListPrefs> arc_app_list_prefs_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ArcVmmManagerBrowserTest, Ctor) {
  EXPECT_TRUE(arc_connected());
}

}  // namespace arc
