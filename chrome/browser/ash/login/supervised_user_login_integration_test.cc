// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/supervised_user_login_delegate.h"

// Tests using production GAIA can only run on branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

// This class implements CrOS login using prod GAIA with the different types of
// supervised accounts.
class SupervisedUserLoginIntegrationTest : public AshIntegrationTest {
 public:
  SupervisedUserLoginIntegrationTest() {
    set_exit_when_last_browser_closes(false);

    // Allows network access for production Gaia.
    SetAllowNetworkAccessToHostResolutions();

    login_mixin().SetMode(
        ChromeOSIntegrationLoginMixin::Mode::kCustomGaiaLogin);
    login_mixin().set_custom_gaia_login_delegate(&delegate_);
  }

 protected:
  SupervisedUserLoginDelegate delegate_;
};

IN_PROC_BROWSER_TEST_F(SupervisedUserLoginIntegrationTest, TestUnicornLogin) {
  login_mixin().Login();

  ash::test::WaitForPrimaryUserSessionStart();

  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserLoginIntegrationTest, TestGellerLogin) {
  delegate_.set_user_type(
      SupervisedUserLoginDelegate::SupervisedUserType::kGeller);
  login_mixin().Login();

  ash::test::WaitForPrimaryUserSessionStart();

  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserLoginIntegrationTest, TestGriffinLogin) {
  delegate_.set_user_type(
      SupervisedUserLoginDelegate::SupervisedUserType::kGriffin);
  login_mixin().Login();

  ash::test::WaitForPrimaryUserSessionStart();

  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
