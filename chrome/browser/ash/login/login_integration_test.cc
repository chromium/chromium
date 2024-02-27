// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"

class LoginIntegrationTest : public AshIntegrationTest {
 public:
  LoginIntegrationTest() {
    set_exit_when_last_browser_closes(false);

    login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kTestLogin);
  }

  LoginIntegrationTest(const LoginIntegrationTest&) = delete;
  LoginIntegrationTest& operator=(const LoginIntegrationTest&) = delete;

  ~LoginIntegrationTest() override = default;
};

IN_PROC_BROWSER_TEST_F(LoginIntegrationTest, TestLogin) {
  login_mixin().Login();

  // Waits for the primary user session to start.
  ash::test::WaitForPrimaryUserSessionStart();

  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Gaia login is only supported for branded build.
class GaiaLoginIntegrationTest : public AshIntegrationTest {
 public:
  GaiaLoginIntegrationTest() {
    set_exit_when_last_browser_closes(false);

    // Allows network access for production Gaia.
    SetAllowNetworkAccessToHostResolutions();

    login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kGaiaLogin);
  }

  GaiaLoginIntegrationTest(const GaiaLoginIntegrationTest&) = delete;
  GaiaLoginIntegrationTest& operator=(const GaiaLoginIntegrationTest&) = delete;

  ~GaiaLoginIntegrationTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GaiaLoginIntegrationTest, GaiaLogin) {
  login_mixin().Login();

  // Waits for the primary user session to start.
  ash::test::WaitForPrimaryUserSessionStart();

  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
