// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using LoginStateApitest = ExtensionApiTest;

// Test that |loginState.getProfileType()| returns |USER_PROFILE| for
// extensions not running in the signin profile.
IN_PROC_BROWSER_TEST_F(LoginStateApitest, GetProfileType_UserProfile) {
  EXPECT_TRUE(RunExtensionTest("login_screen_apis/login_state/get_profile_type",
                               {.custom_arg = "USER_PROFILE"}));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test that |loginState.getSessionState()| returns |IN_SESSION| for extensions
// not running on the login screen.
IN_PROC_BROWSER_TEST_F(LoginStateApitest, GetSessionState_InSession) {
  EXPECT_TRUE(
      RunExtensionTest("login_screen_apis/login_state/get_session_state",
                       {.custom_arg = "IN_SESSION"}));
}
#endif

}  // namespace extensions
