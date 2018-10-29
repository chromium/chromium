// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_feedback.h"

#include <memory>

#include "apps/test/app_window_waiter.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/base_window.h"

namespace chromeos {

class LoginFeedbackTest : public LoginManagerTest {
 public:
  LoginFeedbackTest() : LoginManagerTest(true, true) {}
  ~LoginFeedbackTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginFeedbackTest);
};

// Test feedback UI shows up and is active.
IN_PROC_BROWSER_TEST_F(LoginFeedbackTest, Basic) {
  Profile* const profile = ProfileHelper::GetSigninProfile();
  std::unique_ptr<LoginFeedback> login_feedback(new LoginFeedback(profile));

  base::RunLoop run_loop;
  login_feedback->Request("Test feedback", run_loop.QuitClosure());

  extensions::AppWindow* feedback_window =
      apps::AppWindowWaiter(extensions::AppWindowRegistry::Get(profile),
                            extension_misc::kFeedbackExtensionId)
          .WaitForShown();
  ASSERT_NE(nullptr, feedback_window);
  EXPECT_FALSE(feedback_window->is_hidden());

  EXPECT_TRUE(feedback_window->GetBaseWindow()->IsActive());

  feedback_window->GetBaseWindow()->Close();
  run_loop.Run();
}

}  // namespace chromeos
