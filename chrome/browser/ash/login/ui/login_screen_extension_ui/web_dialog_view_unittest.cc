// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_screen_extension_ui/web_dialog_view.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/ui/login_screen_extension_ui/create_options.h"
#include "chrome/browser/ash/login/ui/login_screen_extension_ui/dialog_delegate.h"
#include "chrome/browser/ui/ash/test_login_screen.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MockLoginScreen : public TestLoginScreen {
 public:
  MockLoginScreen() = default;
  ~MockLoginScreen() override = default;

  MOCK_METHOD1(FocusLoginShelf, void(bool reverse));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLoginScreen);
};

}  // namespace

namespace chromeos {

namespace login_screen_extension_ui {

class LoginScreenExtensionUiWebDialogViewUnittest : public testing::Test {
 public:
  LoginScreenExtensionUiWebDialogViewUnittest() = default;
  ~LoginScreenExtensionUiWebDialogViewUnittest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile;
  testing::StrictMock<MockLoginScreen> mock_login_screen_;

  std::unique_ptr<DialogDelegate> dialog_delegate_;
  std::unique_ptr<WebDialogView> dialog_view_;

  void CreateDialogView(bool can_be_closed_by_user = true) {
    CreateOptions create_options("extension_name", GURL(),
                                 can_be_closed_by_user,
                                 /*close_callback=*/base::DoNothing());

    dialog_delegate_ = std::make_unique<DialogDelegate>(&create_options);

    dialog_view_ = std::make_unique<WebDialogView>(
        &profile, dialog_delegate_.get(),
        std::make_unique<ChromeWebContentsHandler>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenExtensionUiWebDialogViewUnittest);
};

TEST_F(LoginScreenExtensionUiWebDialogViewUnittest, ShouldShowCloseButton) {
  CreateDialogView(/*can_be_closed_by_user=*/true);
  EXPECT_TRUE(dialog_view_->ShouldShowCloseButton());
}

TEST_F(LoginScreenExtensionUiWebDialogViewUnittest, ShouldNotShowCloseButton) {
  CreateDialogView(/*can_be_closed_by_user=*/false);
  EXPECT_FALSE(dialog_view_->ShouldShowCloseButton());
}

TEST_F(LoginScreenExtensionUiWebDialogViewUnittest,
       ShouldCenterDialogTitleText) {
  CreateDialogView(/*can_be_closed_by_user=*/false);
  EXPECT_TRUE(dialog_view_->ShouldCenterDialogTitleText());
}

TEST_F(LoginScreenExtensionUiWebDialogViewUnittest, TabOut) {
  CreateDialogView();

  EXPECT_CALL(mock_login_screen_, FocusLoginShelf(/*reverse=*/true));
  EXPECT_TRUE(dialog_view_->TakeFocus(nullptr, /*reverse=*/true));
  testing::Mock::VerifyAndClearExpectations(&mock_login_screen_);

  EXPECT_CALL(mock_login_screen_, FocusLoginShelf(/*reverse=*/false));
  EXPECT_TRUE(dialog_view_->TakeFocus(nullptr, /*reverse=*/false));
  testing::Mock::VerifyAndClearExpectations(&mock_login_screen_);
}

}  // namespace login_screen_extension_ui

}  // namespace chromeos
