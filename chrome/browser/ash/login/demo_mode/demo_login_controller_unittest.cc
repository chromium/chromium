// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_login_controller.h"

#include "ash/login/test_login_screen.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ui/ash/login/mock_login_display_host.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kValidGaiaCreds[] =
    R"({
      "username":"example@gmail.com",
      "obfuscatedGaiaId":"123",
      "authorizationCode":"abc"
    })";

constexpr char kInValidGaiaCreds[] =
    R"({
      "username":"example@gmail.com",
      "gaia_id":"123"
    })";

constexpr char kSetupDemoAccountUrl[] =
    "https://demomode-pa.googleapis.com/v1/accounts";

constexpr char kApiKeyParam[] = "key";

}  // namespace

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

    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
  }

  void TearDown() override {
    demo_login_controller_.reset();
    login_screen_client_.reset();
  }

  GURL GetSetupUrl() {
    return net::AppendQueryParameter(GURL(kSetupDemoAccountUrl), kApiKeyParam,
                                     google_apis::GetAPIKey());
  }

  network::TestURLLoaderFactory test_url_loader_factory_;

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
  test_url_loader_factory_.AddResponse(GetSetupUrl().spec(), kValidGaiaCreds);

  base::RunLoop loop;
  EXPECT_CALL(login_display_host(), CompleteLogin)
      .Times(1)
      .WillOnce(testing::Invoke([&](const UserContext& user_context) {
        EXPECT_FALSE(user_context.GetDeviceId().empty());
        loop.Quit();
      }));
  login_screen_client()->OnLoginScreenShown();
  loop.Run();
}

TEST_F(DemoLoginControllerTest, InValidGaia) {
  test_url_loader_factory_.AddResponse(GetSetupUrl().spec(), kInValidGaiaCreds);

  base::RunLoop loop;
  EXPECT_CALL(login_display_host(), CompleteLogin).Times(0);
  demo_login_controller()->SetSetupFailedCallbackForTest(
      base::BindLambdaForTesting(
          [&](const DemoLoginController::ResultCode result_code) {
            EXPECT_EQ(result_code,
                      DemoLoginController::ResultCode::kInvalidCreds);
            loop.Quit();
          }));
  login_screen_client()->OnLoginScreenShown();
  loop.Run();
}

// TODO(crbug.com/372771485): Add more request fail test cases.

}  // namespace ash
