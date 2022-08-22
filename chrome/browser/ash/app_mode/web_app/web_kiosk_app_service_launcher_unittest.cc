// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"
#include <sys/types.h>

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::Return;

namespace ash {

namespace {

class MockAppLauncherDelegate : public WebKioskAppServiceLauncher::Delegate {
 public:
  MockAppLauncherDelegate() = default;
  ~MockAppLauncherDelegate() override = default;

  MOCK_METHOD0(InitializeNetwork, void());
  MOCK_METHOD0(OnAppInstalling, void());
  MOCK_METHOD0(OnAppPrepared, void());
  MOCK_METHOD0(OnAppLaunched, void());
  MOCK_METHOD0(OnAppWindowCreated, void());
  MOCK_METHOD1(OnLaunchFailed, void(KioskAppLaunchError::Error));

  MOCK_CONST_METHOD0(IsNetworkReady, bool());
  MOCK_CONST_METHOD0(IsShowingNetworkConfigScreen, bool());
  MOCK_CONST_METHOD0(ShouldSkipAppInstallation, bool());
};

const char kAppEmail[] = "lala@example.com";
const char kAppInstallUrl[] = "https://example.com";
const char kAppLaunchUrl[] = "https://example.com/launch";
const char16_t kAppTitle[] = u"app";

}  // namespace

class WebKioskAppServiceLauncherTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    app_service_test_.UninstallAllApps(profile());
    app_service_test_.SetUp(profile());
    app_service_test_.WaitForAppService();
    app_service_ = apps::AppServiceProxyFactory::GetForProfile(profile());

    fake_web_app_provider_ = web_app::FakeWebAppProvider::Get(profile());
    fake_web_app_provider_->SetDefaultFakeSubsystems();
    fake_web_app_provider_->SetRunSubsystemStartupTasks(true);

    auto fake_externally_managed_app_manager =
        std::make_unique<web_app::FakeExternallyManagedAppManager>(profile());
    fake_web_app_provider_->SetExternallyManagedAppManager(
        std::move(fake_externally_managed_app_manager));

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    externally_managed_app_manager().SetSubsystems(
        &app_registrar(), /*ui_manager=*/nullptr,
        /*finalizer=*/nullptr, /*command_manager=*/nullptr,
        /*sync_bridge=*/nullptr);
    externally_managed_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting(
            [this](const web_app::ExternalInstallOptions& install_options)
                -> web_app::ExternallyManagedAppManager::InstallResult {
              if (!webapps::IsSuccess(install_result_code_)) {
                return web_app::ExternallyManagedAppManager::InstallResult(
                    install_result_code_);
              }
              const GURL start_url = GURL(kAppLaunchUrl);
              const GURL& install_url = install_options.install_url;
              const web_app::AppId app_id = web_app::GenerateAppId(
                  /*manifest_id=*/absl::nullopt, start_url);
              if (!app_registrar().GetAppById(app_id)) {
                const auto install_source = install_options.install_source;
                std::unique_ptr<web_app::WebApp> web_app =
                    web_app::test::CreateWebApp(
                        start_url,
                        ConvertExternalInstallSourceToSource(install_source));
                web_app->SetName(base::UTF16ToUTF8(kAppTitle));
                RegisterApp(std::move(web_app));
                web_app::test::AddInstallUrlData(profile()->GetPrefs(),
                                                 &sync_bridge(), app_id,
                                                 install_url, install_source);
              }
              return web_app::ExternallyManagedAppManager::InstallResult(
                  install_result_code_, app_id);
            }));

    // Web app system checks for system web app info before launching.
    system_web_app_manager()->SetSubsystems(
        &externally_managed_app_manager(), &app_registrar(), &sync_bridge(),
        &ui_manager(), /*web_app_policy_manager=*/nullptr);

    web_app::WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
        base::BindLambdaForTesting(
            [this](apps::AppLaunchParams&& params) -> content::WebContents* {
              auto instance = std::make_unique<apps::Instance>(
                  params.app_id, base::UnguessableToken(), /*window=*/nullptr);
              app_service()->InstanceRegistry().OnInstance(std::move(instance));
              return nullptr;
            }));

    app_manager_ = std::make_unique<WebKioskAppManager>();

    delegate_ = std::make_unique<MockAppLauncherDelegate>();
    launcher_ = std::make_unique<WebKioskAppServiceLauncher>(
        profile(), delegate_.get(), AccountId::FromUserEmail(kAppEmail));
  }

  void TearDown() override {
    launcher_.reset();
    delegate_.reset();
    app_manager_.reset();
    fake_web_app_provider_->Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  void SetupAppData(bool installed) {
    account_id_ = AccountId::FromUserEmail(kAppEmail);
    app_manager_->AddAppForTesting(account_id_, GURL(kAppInstallUrl));

    if (installed) {
      {
        base::RunLoop loop;
        web_app::ExternalInstallOptions install_options(
            GURL(kAppInstallUrl), web_app::UserDisplayMode::kStandalone,
            web_app::ExternalInstallSource::kKiosk);
        externally_managed_app_manager().Install(
            install_options,
            base::BindLambdaForTesting(
                [&loop](const GURL& install_url,
                        web_app::ExternallyManagedAppManager::InstallResult
                            result) {
                  ASSERT_TRUE(webapps::IsSuccess(result.code));
                  loop.Quit();
                }));
        loop.Run();
      }

      WebAppInstallInfo info;
      info.start_url = GURL(kAppLaunchUrl);
      info.title = kAppTitle;
      app_manager_->UpdateAppByAccountId(account_id_, info);
    }
  }

  const WebKioskAppData* app_data() {
    return app_manager_->GetAppByAccountId(account_id_);
  }

  MockAppLauncherDelegate* delegate() { return delegate_.get(); }
  KioskAppLauncher* launcher() { return launcher_.get(); }

 protected:
  void RegisterApp(std::unique_ptr<web_app::WebApp> web_app) {
    std::string app_id = web_app->app_id();
    std::unique_ptr<web_app::WebAppRegistryUpdate> update =
        sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
    sync_bridge().CommitUpdate(
        std::move(update),
        base::BindOnce(&WebKioskAppServiceLauncherTest::OnAppRegistered,
                       base::Unretained(this), std::move(app_id)));
  }

  apps::AppServiceProxy* app_service() { return app_service_; }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

  web_app::WebAppProvider* web_app_provider() {
    return web_app::WebAppProvider::GetForTest(profile());
  }

  web_app::WebAppRegistrar& app_registrar() {
    return web_app_provider()->registrar();
  }

  web_app::WebAppSyncBridge& sync_bridge() {
    return web_app_provider()->sync_bridge();
  }

  web_app::WebAppUiManager& ui_manager() {
    return web_app_provider()->ui_manager();
  }

  SystemWebAppManager* system_web_app_manager() {
    return SystemWebAppManager::GetForTest(profile());
  }

  web_app::FakeExternallyManagedAppManager& externally_managed_app_manager() {
    return static_cast<web_app::FakeExternallyManagedAppManager&>(
        web_app_provider()->externally_managed_app_manager());
  }

  void set_install_result_code(webapps::InstallResultCode result_code) {
    install_result_code_ = result_code;
  }

 private:
  void OnAppRegistered(std::string app_id, bool success) {
    ASSERT_TRUE(success);
    web_app_provider()->install_manager().NotifyWebAppInstalled(app_id);
  }

  AccountId account_id_;

  apps::AppServiceTest app_service_test_;
  apps::AppServiceProxy* app_service_ = nullptr;

  std::unique_ptr<WebKioskAppManager> app_manager_;

  raw_ptr<web_app::FakeWebAppProvider> fake_web_app_provider_;

  std::unique_ptr<MockAppLauncherDelegate> delegate_;
  std::unique_ptr<WebKioskAppServiceLauncher> launcher_;

  webapps::InstallResultCode install_result_code_ =
      webapps::InstallResultCode::kSuccessNewInstall;
};

TEST_F(WebKioskAppServiceLauncherTest, NormalFlowNotInstalled) {
  SetupAppData(/*installed*/ false);
  {
    base::RunLoop loop;
    EXPECT_CALL(*delegate(), InitializeNetwork())
        .WillOnce(RunClosure(loop.QuitClosure()));
    launcher()->Initialize();
    loop.Run();
  }
  {
    base::RunLoop loop;
    EXPECT_CALL(*delegate(), OnAppInstalling());
    EXPECT_CALL(*delegate(), OnAppPrepared())
        .WillOnce(RunClosure(loop.QuitClosure()));
    launcher()->ContinueWithNetworkReady();
    loop.Run();
  }
  {
    base::RunLoop loop;
    EXPECT_CALL(*delegate(), OnAppLaunched())
        .WillOnce(RunClosure(loop.QuitClosure()));
    launcher()->LaunchApp();
    loop.Run();
  }
}

TEST_F(WebKioskAppServiceLauncherTest, NormalFlowAlreadyInstalled) {
  SetupAppData(/*installed*/ true);
  {
    base::RunLoop loop;
    EXPECT_CALL(*delegate(), OnAppPrepared())
        .WillOnce(RunClosure(loop.QuitClosure()));
    launcher()->Initialize();
    loop.Run();
  }
  {
    base::RunLoop loop;
    EXPECT_CALL(*delegate(), OnAppLaunched())
        .WillOnce(RunClosure(loop.QuitClosure()));
    launcher()->LaunchApp();
    loop.Run();
  }
}

TEST_F(WebKioskAppServiceLauncherTest, FailedToInstall) {
  SetupAppData(/*installed*/ false);
  {
    base::RunLoop loop;
    EXPECT_CALL(*delegate(), InitializeNetwork())
        .WillOnce(RunClosure(loop.QuitClosure()));
    launcher()->Initialize();
    loop.Run();
  }
  set_install_result_code(webapps::InstallResultCode::kInstallURLLoadFailed);
  {
    base::RunLoop loop;
    EXPECT_CALL(*delegate(), OnAppInstalling());
    EXPECT_CALL(*delegate(),
                OnLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall))
        .WillOnce(RunClosure(loop.QuitClosure()));
    launcher()->ContinueWithNetworkReady();
    loop.Run();
  }
  EXPECT_NE(app_data()->status(), WebKioskAppData::Status::kInstalled);
}

}  // namespace ash
