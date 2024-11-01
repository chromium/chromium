// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_login_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/login/test_login_screen.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ui/ash/login/mock_login_display_host.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kValidGaiaCreds[] =
    R"({
      "username":"example@gmail.com",
      "obfuscatedGaiaId":"%s",
      "authorizationCode":"abc"
    })";

constexpr char kInValidGaiaCreds[] =
    R"({
      "username":"example@gmail.com",
      "gaia_id":"123"
    })";

constexpr char kSetupDemoAccountUrl[] =
    "https://demomode-pa.googleapis.com/v1/accounts";

constexpr char kCleanUpDemoAccountUrl[] =
    "https://demomode-pa.googleapis.com/v1/accounts:remove";

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

  GURL GetCleanUpUrl() {
    return net::AppendQueryParameter(GURL(kCleanUpDemoAccountUrl), kApiKeyParam,
                                     google_apis::GetAPIKey());
  }

  // Mock a setup response return provided `gaia_id`. Verify that setup request
  // gets triggered and login is success.
  void MockSuccessSetupResponseAndVerifyLogin(const std::string& gaia_id) {
    // Mock a setup request will be success.
    test_url_loader_factory_.AddResponse(
        GetSetupUrl().spec(), base::StringPrintf(kValidGaiaCreds, gaia_id));
    // Expect login if after clean up success.
    base::RunLoop loop;
    EXPECT_CALL(login_display_host(), CompleteLogin)
        .Times(1)
        .WillOnce(testing::Invoke([&](const UserContext& user_context) {
          EXPECT_FALSE(user_context.GetDeviceId().empty());
          EXPECT_EQ(g_browser_process->local_state()->GetString(
                        prefs::kDemoAccountGaiaId),
                    gaia_id);
          loop.Quit();
        }));
    loop.Run();
  }

  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedStubInstallAttributes attributes_;
  testing::NiceMock<ash::MockLoginDisplayHost> mock_login_display_host_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};

  // Dependencies for `LoginScreenClientImpl`:
  session_manager::SessionManager session_manager_;
  TestLoginScreen test_login_screen_;
  std::unique_ptr<LoginScreenClientImpl> login_screen_client_;

  std::unique_ptr<DemoLoginController> demo_login_controller_;
};

TEST_F(DemoLoginControllerTest, OnSetupDemoAccountSuccessFirstTime) {
  const std::string gaia_id = "123";
  test_url_loader_factory_.AddResponse(
      GetSetupUrl().spec(), base::StringPrintf(kValidGaiaCreds, gaia_id));

  base::RunLoop loop;
  EXPECT_CALL(login_display_host(), CompleteLogin)
      .Times(1)
      .WillOnce(testing::Invoke([&](const UserContext& user_context) {
        EXPECT_FALSE(user_context.GetDeviceId().empty());
        EXPECT_EQ(g_browser_process->local_state()->GetString(
                      prefs::kDemoAccountGaiaId),
                  gaia_id);
        loop.Quit();
      }));
  login_screen_client()->OnLoginScreenShown();
  // For first time setup demo account, no clean up get triggered.
  ASSERT_FALSE(test_url_loader_factory_.IsPending(GetCleanUpUrl().spec()));
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

TEST_F(DemoLoginControllerTest, CleanUpSuccess) {
  g_browser_process->local_state()->SetString(prefs::kDemoAccountGaiaId, "123");
  base::MockCallback<DemoLoginController::FailedRequestCallback>
      cleanup_failed_callback;
  // `cleanup_failed_callback` is not called means no failure for clean up.
  EXPECT_CALL(cleanup_failed_callback, Run(testing::_)).Times(0);
  demo_login_controller()->SetCleanUpFailedCallbackForTest(
      cleanup_failed_callback.Get());

  login_screen_client()->OnLoginScreenShown();

  // Verify the request was sent.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(GetCleanUpUrl().spec()));
  test_url_loader_factory_.AddResponse(GetCleanUpUrl().spec(), "{}");

  MockSuccessSetupResponseAndVerifyLogin(/*gaia_id=*/"234");
}

TEST_F(DemoLoginControllerTest, CleanUpFailed) {
  g_browser_process->local_state()->SetString(prefs::kDemoAccountGaiaId, "123");
  test_url_loader_factory_.AddResponse(GetCleanUpUrl().spec(), "{}",
                                       net::HTTP_UNAUTHORIZED);
  base::RunLoop loop;
  demo_login_controller()->SetCleanUpFailedCallbackForTest(
      base::BindLambdaForTesting(
          [&](const DemoLoginController::ResultCode result_code) {
            EXPECT_EQ(result_code,
                      DemoLoginController::ResultCode::kRequestFailed);
            loop.Quit();
          }));

  // Verify login screen shown will trigger clean up and `loop` will quick on
  // fail callback gets invoked.
  login_screen_client()->OnLoginScreenShown();
  loop.Run();

  // Verify login:
  MockSuccessSetupResponseAndVerifyLogin(/*gaia_id=*/"234");
}

// TODO(crbug.com/372771485): Add more request fail test cases.

}  // namespace ash
