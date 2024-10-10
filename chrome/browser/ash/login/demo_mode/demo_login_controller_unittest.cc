// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_login_controller.h"

#include "ash/login/test_login_screen.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ui/ash/login/mock_login_display_host.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kValidGaiaCreds[] =
    R"({
      "username":"example@gmail.com",
      "gaiaId":"123",
      "authorizationCode":"abc"
    })";
}

class DemoLoginControllerTest : public testing::Test {
 protected:
  ash::MockLoginDisplayHost& login_display_host() {
    return mock_login_display_host_;
  }

  DemoLoginController* demo_login_controller() {
    return demo_login_controller_.get();
  }

  LoginScreenClientImpl* login_screen_client() {
    return login_screen_client_.get();
  }

  void SetUp() override {
    attributes_.Get()->SetDemoMode();
    login_screen_client_ = std::make_unique<LoginScreenClientImpl>();
    demo_login_controller_ =
        std::make_unique<DemoLoginController>(login_screen_client_.get());
  }

  void TearDown() override {
    demo_login_controller_.reset();
    login_screen_client_.reset();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedStubInstallAttributes attributes_;
  testing::NiceMock<ash::MockLoginDisplayHost> mock_login_display_host_;

  // Dependencies for `LoginScreenClientImpl`:
  session_manager::SessionManager session_manager_;
  TestLoginScreen test_login_screen_;
  std::unique_ptr<LoginScreenClientImpl> login_screen_client_;
  std::unique_ptr<DemoLoginController> demo_login_controller_;
};

TEST_F(DemoLoginControllerTest, OnSetupDemoAccountSuccess) {
  demo_login_controller()->SetSetupDemoAccountResponseForTest(kValidGaiaCreds);
  EXPECT_CALL(login_display_host(), CompleteLogin);
  login_screen_client()->OnLoginScreenShown();
}

TEST_F(DemoLoginControllerTest, OnSetupDemoAccountFail) {
  const auto invalid_creds = base::StringPrintf(R"(
    {
      "username":"example@gmail.com",
      "gaiaId":"123"
    }
)");
  demo_login_controller()->SetSetupDemoAccountResponseForTest(
      invalid_creds.c_str());

  EXPECT_CALL(login_display_host(), CompleteLogin).Times(0);
  login_screen_client()->OnLoginScreenShown();
}

}  // namespace ash
