// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/growth/install_web_app_action_performer.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestProfileName[] = "user@gmail.com";

constexpr char kAppInstallTemplate[] = R"(
    {
      "appTitle": "Test App 1",
      "%s": "%s",
      "iconPath": "https://www.test.com/icon",
      "launchInStandaloneWindow": true
    }
)";

constexpr char kValidURL[] = "https://www.news.com";
constexpr char kExpectedAppId[] = "pelbleffcebihhhneibojlacmlmlnjhm";
constexpr char kInvalidURL[] = "www";
constexpr char kValidURLKey[] = "url";
constexpr char kInvalidURLKey[] = "urlInvalid";

}  // namespace

class InstallWebAppActionPerformerTest : public testing::Test {
 public:
  InstallWebAppActionPerformerTest() = default;
  InstallWebAppActionPerformerTest(const InstallWebAppActionPerformerTest&) =
      delete;
  InstallWebAppActionPerformerTest& operator=(
      const InstallWebAppActionPerformerTest&) = delete;
  ~InstallWebAppActionPerformerTest() override = default;

  void SetUp() override {
    user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    const user_manager::User* user =
        user_manager_->AddUser(AccountId::FromUserEmail(kTestProfileName));
    user_manager_->LoginUser(user->GetAccountId());
    user_manager_->SwitchActiveUser(user->GetAccountId());

    // Note that user profiles are created after user login in reality.
    profile_ = profile_manager_->CreateTestingProfile(kTestProfileName);

    auto* provider = web_app::FakeWebAppProvider::Get(profile_);
    provider->SetOsIntegrationManager(
        std::make_unique<web_app::FakeOsIntegrationManager>(
            profile_,
            /*file_handler_manager=*/nullptr,
            /*protocol_handler_manager=*/nullptr));
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_);
    install_action_ = std::make_unique<InstallWebAppActionPerformer>();
  }

  void TearDown() override {
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  bool EnsureWebAppInstalled(const std::string& url) {
    apps_installed_run_loop_.Run();

    // Let's verify that the pwa was installed.
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForWebApps(profile_);

    // note registrar_unsafe() is fine since we are just
    // running a simple test here.
    const auto& registrar = provider->registrar_unsafe();
    return registrar.GetAppLaunchUrl(kExpectedAppId) == GURL(url);
  }

  bool EnsureAppsInstallActionFailed() {
    action_failed_run_loop_.Run();
    return true;
  }

  void InstallWebAppActionPerformerCallback(
      growth::ActionResult result,
      std::optional<growth::ActionResultReason> reason) {
    if (result == growth::ActionResult::kSuccess) {
      std::move(app_installed_closure_).Run();
    } else {
      std::move(install_action_failed_closure_).Run();
    }
  }

  InstallWebAppActionPerformer& action() { return *install_action_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  base::RunLoop apps_installed_run_loop_;
  base::RunLoop action_failed_run_loop_;

  base::OnceClosure app_installed_closure_ =
      apps_installed_run_loop_.QuitClosure();
  base::OnceClosure install_action_failed_closure_ =
      action_failed_run_loop_.QuitClosure();

  raw_ptr<Profile> profile_ = nullptr;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;
  std::unique_ptr<InstallWebAppActionPerformer> install_action_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(InstallWebAppActionPerformerTest, TestValidInstallation) {
  const auto validInstallDictString =
      base::StringPrintf(kAppInstallTemplate, kValidURLKey, kValidURL);
  auto value = base::JSONReader::Read(validInstallDictString);
  ASSERT_TRUE(value.has_value());
  action().Run(/*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
               base::BindOnce(&InstallWebAppActionPerformerTest::
                                  InstallWebAppActionPerformerCallback,
                              base::Unretained(this)));
  EXPECT_TRUE(EnsureWebAppInstalled(kValidURL));
}

TEST_F(InstallWebAppActionPerformerTest, TestInvalidInstallationInvalidURL) {
  const auto invalidInstallDictString =
      base::StringPrintf(kAppInstallTemplate, kValidURLKey, kInvalidURL);
  auto value = base::JSONReader::Read(invalidInstallDictString);
  ASSERT_TRUE(value.has_value());

  action().Run(/*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
               base::BindOnce(&InstallWebAppActionPerformerTest::
                                  InstallWebAppActionPerformerCallback,
                              base::Unretained(this)));
  EXPECT_TRUE(EnsureAppsInstallActionFailed());
}

TEST_F(InstallWebAppActionPerformerTest, TestInvalidUrlKey) {
  const auto invalidInstallDictString =
      base::StringPrintf(kAppInstallTemplate, kInvalidURLKey, kValidURL);
  auto value = base::JSONReader::Read(invalidInstallDictString);
  ASSERT_TRUE(value.has_value());

  action().Run(/*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
               base::BindOnce(&InstallWebAppActionPerformerTest::
                                  InstallWebAppActionPerformerCallback,
                              base::Unretained(this)));
  EXPECT_TRUE(EnsureAppsInstallActionFailed());
}

TEST_F(InstallWebAppActionPerformerTest, InvalidRequest) {
  constexpr char kInvalidParams[] = R"({
      "invalidInstallWebAppParams" : {
        "param" : "param"
      }
    })";
  auto value = base::JSONReader::Read(kInvalidParams);
  ASSERT_TRUE(value.has_value());
  action().Run(/*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
               base::BindOnce(&InstallWebAppActionPerformerTest::
                                  InstallWebAppActionPerformerCallback,
                              base::Unretained(this)));
  EXPECT_TRUE(EnsureAppsInstallActionFailed());
}
