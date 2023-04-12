// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/login/app_mode/test/ash_accelerator_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

// Helper based on AshBrowserTestStarter, sets up Lacros in a kiosk session.
class KioskAshBrowserTestStarter {
 public:
  bool HasLacrosArgument() {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        ash::switches::kLacrosChromePath);
  }
  // Must be called in SetUpInProcessBrowserTestFixture.
  void PrepareEnvironmentForKioskLacros() {
    DCHECK(HasLacrosArgument());
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    ASSERT_TRUE(scoped_temp_dir_xdg_.CreateUniqueTempDir());
    env->SetVar("XDG_RUNTIME_DIR",
                scoped_temp_dir_xdg_.GetPath().AsUTF8Unsafe());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshEnableWaylandServer);
  }

  // Must be called in SetUpOnMainThread.
  void SetLacrosAvailabilityPolicy() {
    DCHECK(HasLacrosArgument());
    policy::PolicyMap policy;
    policy.Set(policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(GetLacrosAvailabilityPolicyName(
                   ash::standalone_browser::LacrosAvailability::kLacrosOnly)),
               /*external_data_fetcher=*/nullptr);
    crosapi::browser_util::CacheLacrosAvailability(policy);
  }

 private:
  base::ScopedTempDir scoped_temp_dir_xdg_;
};

// Observes BrowserServiceHostAsh and blocks until a BrowserService disconnects.
class BrowserServiceDisconnectedWaiter
    : public crosapi::BrowserServiceHostObserver {
 public:
  BrowserServiceDisconnectedWaiter() {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->AddObserver(this);
  }

  ~BrowserServiceDisconnectedWaiter() override {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->RemoveObserver(this);
  }

  [[nodiscard]] bool Wait() { return future_.Wait(); }

 private:
  void OnBrowserServiceDisconnected(crosapi::CrosapiId id,
                                    mojo::RemoteSetElementId mojo_id) override {
    future_.SetValue();
  }

  base::test::TestFuture<void> future_;
};

[[nodiscard]] bool WaitUntilBrowserServiceDisconnected() {
  BrowserServiceDisconnectedWaiter waiter;
  return waiter.Wait();
}

}  // namespace

// Tests system accelerators (ash-side) do not work with Lacros in kiosk.
class WebKioskAcceleratorLacrosTest : public WebKioskBaseTest {
  void SetUpInProcessBrowserTestFixture() override {
    if (kiosk_ash_starter_.HasLacrosArgument()) {
      kiosk_ash_starter_.PrepareEnvironmentForKioskLacros();
    }
    WebKioskBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    WebKioskBaseTest::SetUpOnMainThread();
    if (kiosk_ash_starter_.HasLacrosArgument()) {
      kiosk_ash_starter_.SetLacrosAvailabilityPolicy();
    }
  }

 protected:
  KioskAshBrowserTestStarter kiosk_ash_starter_;
};

IN_PROC_BROWSER_TEST_F(WebKioskAcceleratorLacrosTest, SignOutDoesNotWork) {
  if (!kiosk_ash_starter_.HasLacrosArgument()) {
    return;
  }

  InitializeRegularOnlineKiosk();
  ASSERT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
  ASSERT_FALSE(ash::PressSignOutAccelerator());
  base::RunLoop loop;
  loop.RunUntilIdle();
  EXPECT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
}

// Tests system accelerators (ash-side) work with Lacros when not in kiosk.
class NonKioskAcceleratorLacrosTest : public InProcessBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    if (ash_starter_.HasLacrosArgument()) {
      ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
    }
  }

  void SetUpOnMainThread() override {
    if (ash_starter_.HasLacrosArgument()) {
      ash_starter_.StartLacros(this);
    }
  }

 protected:
  ::test::AshBrowserTestStarter ash_starter_;
};

IN_PROC_BROWSER_TEST_F(NonKioskAcceleratorLacrosTest, SignOutWorks) {
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }

  // crosapi::BrowserManager::Get()->NewTab();
  ASSERT_TRUE(crosapi::BrowserManager::Get()->IsRunning());
  ASSERT_TRUE(ash::PressSignOutAccelerator());
  ASSERT_TRUE(WaitUntilBrowserServiceDisconnected());
  EXPECT_FALSE(crosapi::BrowserManager::Get()->IsRunning());
}

}  // namespace ash
