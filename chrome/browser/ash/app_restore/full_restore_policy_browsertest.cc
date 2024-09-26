// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/app_restore/features.h"
#include "components/exo/wm_helper.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::full_restore {

class FullRestorePolicyBrowserTest
    : public policy::PolicyTest,
      public ash::ShellObserver,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  // policy::PolicyTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
    command_line->AppendSwitch(switches::kEnableArcVm);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
    wm_helper_ = std::make_unique<exo::WMHelper>();

    policy::PolicyMap policies;
    policies.Set(policy::key::kFullRestoreEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(full_restore_enabled()), nullptr);
    policies.Set(policy::key::kGhostWindowEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(ghost_window_enabled()), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    ash::Shell::Get()->AddShellObserver(this);
  }

  void TearDownOnMainThread() override { PolicyTest::TearDownOnMainThread(); }

  // ash::ShellObserver:
  void OnShellDestroying() override {
    // `wm_helper_` needs to be released before `ash::Shell`.
    wm_helper_.reset();
    ash::Shell::Get()->RemoveShellObserver(this);
  }

  bool full_restore_enabled() const { return std::get<0>(GetParam()); }

  bool ghost_window_enabled() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
};

IN_PROC_BROWSER_TEST_P(FullRestorePolicyBrowserTest,
                       DefaultEnableFullRestoreAndGhostWindow) {
  if (full_restore_enabled()) {
    ASSERT_TRUE(FullRestoreServiceFactory::GetForProfile(browser()->profile()));
  } else {
    ASSERT_FALSE(
        FullRestoreServiceFactory::GetForProfile(browser()->profile()));
  }

  if (ghost_window_enabled()) {
    ASSERT_TRUE(app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(
                    browser()->profile())
                    ->window_handler());
  } else {
    ASSERT_FALSE(app_restore::AppRestoreArcTaskHandlerFactory::GetForProfile(
                     browser()->profile())
                     ->window_handler());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         FullRestorePolicyBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace ash::full_restore
