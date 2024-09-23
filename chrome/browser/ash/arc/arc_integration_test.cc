// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_arc_mixin.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

class ArcIntegrationTest : public AshIntegrationTest {
 public:
  ArcIntegrationTest() {
    // This is needed to keep the browser test running after dismissing login
    // screen. Otherwise, login screen destruction releases its ScopedKeepAlive
    // which triggers shutdown from ShutdownIfNoBrowsers.
    set_exit_when_last_browser_closes(false);

    login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kTestLogin);
    arc_mixin().SetMode(ChromeOSIntegrationArcMixin::Mode::kEnabled);
  }

  ArcIntegrationTest(const ArcIntegrationTest&) = delete;
  ArcIntegrationTest& operator=(const ArcIntegrationTest&) = delete;

  ~ArcIntegrationTest() override = default;

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    login_mixin().Login();
    ash::test::WaitForPrimaryUserSessionStart();
    arc_mixin().WaitForBootAndConnectAdb();
  }
};

IN_PROC_BROWSER_TEST_F(ArcIntegrationTest, CreateWindow) {
  // Test android apps are used by tast and deployed via "tast-local-apks-cros"
  // package.
  const base::FilePath kTestApk(
      "/usr/local/libexec/tast/apks/local/cros/ArcKeyCharacterMapTest.apk");
  ASSERT_TRUE(arc_mixin().InstallApk(kTestApk));

  constexpr char kActivity[] = "org.chromium.arc.testapp.kcm.MainActivity";
  constexpr char kPackage[] = "org.chromium.arc.testapp.kcm";

  aura::Window* window =
      arc_mixin().LaunchAndWaitForWindow(kPackage, kActivity);
  ASSERT_NE(window, nullptr);

  chromeos::AppType window_app_type =
      window->GetProperty(chromeos::kAppTypeKey);
  EXPECT_EQ(window_app_type, chromeos::AppType::ARC_APP);
}
