// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using LoginStateApitest = ExtensionApiTest;

// Test that |loginState.getProfileType()| returns |USER_PROFILE| for
// extensions not running in the signin profile.
IN_PROC_BROWSER_TEST_F(LoginStateApitest, GetProfileType_UserProfile) {
  EXPECT_TRUE(RunExtensionTest(
      {.name = "login_screen_apis/login_state/get_profile_type",
       .custom_arg = "USER_PROFILE"}));
}

}  // namespace extensions
