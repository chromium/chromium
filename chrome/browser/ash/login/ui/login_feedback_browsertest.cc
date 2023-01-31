// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_feedback.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "content/public/test/browser_test.h"

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

void EnsureFeedbackAppUIShown(FeedbackDialog* feedback_dialog,
                              base::OnceClosure callback) {
  auto* widget = feedback_dialog->GetWidget();
  ASSERT_NE(nullptr, widget);
  if (widget->IsActive()) {
    std::move(callback).Run();
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EnsureFeedbackAppUIShown, feedback_dialog,
                       std::move(callback)),
        base::Seconds(1));
  }
}

void TestFeedback() {
  Profile* const profile = ProfileHelper::GetSigninProfile();
  std::unique_ptr<LoginFeedback> login_feedback(new LoginFeedback(profile));

  base::RunLoop run_loop;
  // Test that none feedback dialog exists.
  ASSERT_EQ(nullptr, FeedbackDialog::GetInstanceForTest());

  login_feedback->Request("Test feedback");
  FeedbackDialog* feedback_dialog = FeedbackDialog::GetInstanceForTest();
  // Test that a feedback dialog object has been created.
  ASSERT_NE(nullptr, feedback_dialog);

  // The feedback app starts invisible until after a screenshot has been taken
  // via JS on the UI side. Afterward, JS will send a request to show the app
  // window via a message handler.
  EnsureFeedbackAppUIShown(feedback_dialog, run_loop.QuitClosure());
  run_loop.Run();

  // Test that the feedback app is visible now.
  EXPECT_TRUE(feedback_dialog->GetWidget()->IsVisible());
  // Test that the feedback app window is modal.
  EXPECT_TRUE(feedback_dialog->GetWidget()->IsModal());

  // Close the feedback dialog.
  feedback_dialog->GetWidget()->Close();
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
