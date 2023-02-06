// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace ash {

// Tests Ash-side of the web kiosk when Lacros is enabled.
class WebKioskAshRequiresLacrosTest : public WebKioskBaseTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    if (HasLacrosArgument()) {
      PrepareEnvironmentForLacros();
    }
    WebKioskBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    WebKioskBaseTest::SetUpOnMainThread();
    if (HasLacrosArgument()) {
      SetLacrosAvailability(
          ash::standalone_browser::LacrosAvailability::kLacrosOnly);
    }
  }

  // Returns whether the --lacros-chrome-path is provided.
  // If returns false, we should not do any Lacros related testing
  // because the Lacros instance is not provided.
  bool HasLacrosArgument() const {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        ash::switches::kLacrosChromePath);
  }

 private:
  void SetLacrosAvailability(
      ash::standalone_browser::LacrosAvailability lacros_availability) {
    DCHECK(HasLacrosArgument());
    policy::PolicyMap policy;
    policy.Set(
        policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value(GetLacrosAvailabilityPolicyName(lacros_availability)),
        /*external_data_fetcher=*/nullptr);
    crosapi::browser_util::CacheLacrosAvailability(policy);
  }

  // Prepares ash so it can work with Lacros. Should be called in
  // `SetUpInProcessBrowserTestFixture`.
  void PrepareEnvironmentForLacros() {
    DCHECK(HasLacrosArgument());

    std::unique_ptr<base::Environment> env(base::Environment::Create());
    ASSERT_TRUE(scoped_temp_dir_xdg_.CreateUniqueTempDir());
    env->SetVar("XDG_RUNTIME_DIR",
                scoped_temp_dir_xdg_.GetPath().AsUTF8Unsafe());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableWaylandServer);
  }

  // This is XDG_RUNTIME_DIR.
  base::ScopedTempDir scoped_temp_dir_xdg_;
};

IN_PROC_BROWSER_TEST_F(WebKioskAshRequiresLacrosTest, RegularOnlineKiosk) {
  if (!HasLacrosArgument()) {
    return;
  }
  InitializeRegularOnlineKiosk();
  EXPECT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
}

}  // namespace ash
