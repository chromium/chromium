// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_feedback.h"

#include <memory>

#include "apps/test/app_window_waiter.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/base_window.h"

namespace ash {

class LoginFeedbackTest : public LoginManagerTest {
 public:
  LoginFeedbackTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
  }

  LoginFeedbackTest(const LoginFeedbackTest&) = delete;
  LoginFeedbackTest& operator=(const LoginFeedbackTest&) = delete;

  ~LoginFeedbackTest() override {}

 private:
  LoginManagerMixin login_mixin_{&mixin_host_};
};

void TestFeedback() {
  // TODO(b/185480535): Fix the test for WebUIFeedback
  if (base::FeatureList::IsEnabled(features::kWebUIFeedback))
    GTEST_SKIP() << "Skipped due to timeout with webui feedback.";

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

// Test feedback UI shows up and is active on the Login Screen
IN_PROC_BROWSER_TEST_F(LoginFeedbackTest, Basic) {
  TestFeedback();
}

// Test feedback UI shows up and is active in OOBE
IN_PROC_BROWSER_TEST_F(OobeBaseTest, FeedbackBasic) {
  TestFeedback();
}

}  // namespace ash
