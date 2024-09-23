// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_service_launcher.h"

#include <sys/types.h>

#include <memory>
#include <optional>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/web_contents.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace ash {
namespace {

#define EXEC_AND_WAIT_FOR_CALL(exec, mock, method)    \
  ({                                                  \
    TestFuture<bool> waiter;                          \
    EXPECT_CALL(mock, method).WillOnce(Invoke([&]() { \
      waiter.SetValue(true);                          \
    }));                                              \
    exec;                                             \
    EXPECT_TRUE(waiter.Wait());                       \
  })

class MockAppLauncherDelegate : public KioskAppLauncher::NetworkDelegate {
 public:
  MockAppLauncherDelegate() = default;
  ~MockAppLauncherDelegate() override = default;

  MOCK_METHOD0(InitializeNetwork, void());

  MOCK_CONST_METHOD0(IsNetworkReady, bool());
  MOCK_CONST_METHOD0(IsShowingNetworkConfigScreen, bool());
};

class MockAppLauncherObserver : public KioskAppLauncher::Observer {
 public:
  MockAppLauncherObserver() = default;
  ~MockAppLauncherObserver() override = default;

  MOCK_METHOD0(OnAppInstalling, void());
  MOCK_METHOD0(OnAppPrepared, void());
  MOCK_METHOD0(OnAppLaunched, void());
  MOCK_METHOD(void, OnAppWindowCreated, (const std::optional<std::string>&));
  MOCK_METHOD1(OnLaunchFailed, void(KioskAppLaunchError::Error));
};

const char kAppEmail[] = "lala@example.com";
const char kAppInstallUrl[] = "https://example.com";
const char kAppLaunchUrl[] = "https://example.com/launch";
const char kManifestUrl[] = "https://example.com/manifest.json";
const char16_t kAppTitle[] = u"app";

}  // namespace

class WebKioskAppServiceLauncherTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_KIOSK);

    app_service_test_.UninstallAllApps(profile());
    app_service_test_.SetUp(profile());

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
    static_cast<web_app::FakeWebAppUiManager*>(&web_app_provider().ui_manager())
        ->SetOnLaunchWebAppCallback(app_launch_future_.GetRepeatingCallback());

    app_manager_ = std::make_unique<WebKioskAppManager>();
    account_id_ = AccountId::FromUserEmail(kAppEmail);
    app_manager_->AddAppForTesting(account_id_, GURL(kAppInstallUrl));

    launcher_ = std::make_unique<WebKioskAppServiceLauncher>(
        profile(), AccountId::FromUserEmail(kAppEmail), &delegate_);
    launcher_->AddObserver(&observer_);
  }

  void TearDown() override {
    launcher_.reset();
    app_manager_.reset();
    web_app_provider().Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  webapps::AppId CreateWebAppWithManifest() {
    const GURL install_url = GURL(kAppInstallUrl);
    const GURL manifest_url = GURL(kManifestUrl);
    const GURL start_url = GURL(kAppLaunchUrl);

    auto& install_page_state =
        web_contents_manager().GetOrCreatePageState(install_url);
    install_page_state.url_load_result =
        webapps::WebAppUrlLoaderResult::kUrlLoaded;
    install_page_state.redirection_url = std::nullopt;

    install_page_state.opt_metadata =
        web_app::FakeWebContentsManager::CreateMetadataWithTitle(
            u"Basic app title");

    install_page_state.manifest_url = manifest_url;
    install_page_state.valid_manifest_for_web_app = true;

    install_page_state.manifest_before_default_processing =
        blink::mojom::Manifest::New();
    install_page_state.manifest_before_default_processing->start_url =
        start_url;
    install_page_state.manifest_before_default_processing->id =
        web_app::GenerateManifestIdFromStartUrlOnly(start_url);
    install_page_state.manifest_before_default_processing->display =
        blink::mojom::DisplayMode::kStandalone;
    install_page_state.manifest_before_default_processing->short_name =
        u"Basic app name";

    return web_app::GenerateAppId(/*manifest_id=*/std::nullopt, start_url);
  }

  void InstallAppAsPlaceholder() {
    InstallAppInternal(/*install_app_as_placeholder=*/true);
    // sanity check
    EXPECT_TRUE(IsAppInstalledAsPlaceholder());
  }

  void InstallApp() {
    CreateWebAppWithManifest();
    InstallAppInternal(/*install_app_as_placeholder=*/false);

    GURL start_url(kAppLaunchUrl);
    auto info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    info->title = kAppTitle;
    app_manager_->UpdateAppByAccountId(account_id_, *info);
  }

  bool IsAppInstalledAsPlaceholder() {
    return web_app_provider()
        .registrar_unsafe()
        .LookupPlaceholderAppId(GURL(kAppInstallUrl),
                                web_app::WebAppManagement::Type::kKiosk)
        .has_value();
  }

  apps::AppLaunchParams WaitForWebAppLaunch() {
    return std::get<0>(app_launch_future_.Take());
  }

  MockAppLauncherDelegate& delegate() { return delegate_; }
  MockAppLauncherObserver& observer() { return observer_; }
  WebKioskAppServiceLauncher& launcher() { return *launcher_; }

 private:
  apps::AppServiceProxy* app_service() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  web_app::WebAppProvider& web_app_provider() {
    return *web_app::WebAppProvider::GetForTest(profile());
  }

  web_app::FakeWebContentsManager& web_contents_manager() {
    return static_cast<web_app::FakeWebContentsManager&>(
        web_app_provider().web_contents_manager());
  }

  void InstallAppInternal(bool install_app_as_placeholder) {
    TestFuture<const GURL&, web_app::ExternallyManagedAppManager::InstallResult>
        install_result;
    web_app::ExternalInstallOptions install_options(
        GURL(kAppInstallUrl), web_app::mojom::UserDisplayMode::kStandalone,
        web_app::ExternalInstallSource::kKiosk);
    install_options.install_placeholder = install_app_as_placeholder;

    web_app_provider().externally_managed_app_manager().Install(
        install_options, install_result.GetCallback());
    ASSERT_TRUE(webapps::IsSuccess(install_result.Get<1>().code));
  }

  // To ensure data_decoder instance is available. Removing this will make the
  // unittest flaky (b/300670172).
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  AccountId account_id_;
  base::test::TestFuture<apps::AppLaunchParams,
                         web_app::LaunchWebAppWindowSetting>
      app_launch_future_;

  apps::AppServiceTest app_service_test_;

  std::unique_ptr<WebKioskAppManager> app_manager_;

  MockAppLauncherDelegate delegate_;
  MockAppLauncherObserver observer_;
  std::unique_ptr<WebKioskAppServiceLauncher> launcher_;
};

TEST_F(WebKioskAppServiceLauncherTest,
       AppNotInstalledShouldInvokeInitializeNetwork) {
  // Do not preinstall the app

  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), delegate(),
                         InitializeNetwork());
}

TEST_F(WebKioskAppServiceLauncherTest,
       PlaceholderInstalledShouldInvokeInitializeNetwork) {
  InstallAppAsPlaceholder();

  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), delegate(),
                         InitializeNetwork());
}

TEST_F(WebKioskAppServiceLauncherTest,
       ShouldNotInvokeInitializeNetworkWhenAppIsSuccessfullyInstalled) {
  InstallApp();

  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), observer(), OnAppPrepared());
  EXPECT_CALL(delegate(), InitializeNetwork()).Times(0);
}

TEST_F(
    WebKioskAppServiceLauncherTest,
    ShouldInvokeInitializeNetworkWhenAppIsSuccessfullyInstalledButNotOfflineEnabled) {
  profile()->GetPrefs()->SetBoolean(::prefs::kKioskWebAppOfflineEnabled, false);
  InstallApp();

  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), delegate(),
                         InitializeNetwork());
  EXPECT_CALL(observer(), OnAppPrepared()).Times(0);
}

TEST_F(WebKioskAppServiceLauncherTest,
       InitializeShouldInvokeAppPreparedIfAppAlreadyInstalled) {
  InstallApp();

  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), observer(), OnAppPrepared());
}

TEST_F(WebKioskAppServiceLauncherTest,
       ContinueWithNetworkReadyShouldInvokeOnAppInstalling) {
  // Do not preinstall the app

  launcher().Initialize();

  EXEC_AND_WAIT_FOR_CALL(launcher().ContinueWithNetworkReady(), observer(),
                         OnAppInstalling());
}

TEST_F(WebKioskAppServiceLauncherTest, ShouldAlwaysInstallPlaceholder) {
  // Do not preinstall the app and do not set up a valid web app

  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), delegate(),
                         InitializeNetwork());
  EXEC_AND_WAIT_FOR_CALL(launcher().ContinueWithNetworkReady(), observer(),
                         OnAppPrepared());

  EXPECT_TRUE(IsAppInstalledAsPlaceholder());
}

TEST_F(WebKioskAppServiceLauncherTest, LaunchAppShouldInvokeOnAppLaunched) {
  InstallApp();
  launcher().Initialize();

  EXEC_AND_WAIT_FOR_CALL(launcher().LaunchApp(), observer(), OnAppLaunched());
}

TEST_F(WebKioskAppServiceLauncherTest,
       KioskOriginShouldGetUnlimitedStorageGrantedDuringInstallFlow) {
  launcher().Initialize();

  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      GURL(kAppInstallUrl)));
}

TEST_F(WebKioskAppServiceLauncherTest,
       KioskOriginShouldGetUnlimitedStorageGrantedIfAppAlreadyInstalled) {
  InstallApp();
  launcher().Initialize();

  EXPECT_TRUE(profile()->GetExtensionSpecialStoragePolicy()->IsStorageUnlimited(
      GURL(kAppInstallUrl)));
}

TEST_F(WebKioskAppServiceLauncherTest,
       InstallUrlShouldBeSetAsOverrideUrlInLaunchParams) {
  InstallApp();
  launcher().Initialize();
  launcher().LaunchApp();

  EXPECT_EQ(WaitForWebAppLaunch().override_url, GURL(kAppInstallUrl));
}

TEST_F(WebKioskAppServiceLauncherTest, FullFlowNotInstalled) {
  // Do not preinstall teh app

  base::HistogramTester histogram;

  CreateWebAppWithManifest();

  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), delegate(),
                         InitializeNetwork());
  EXEC_AND_WAIT_FOR_CALL(launcher().ContinueWithNetworkReady(), observer(),
                         OnAppPrepared());
  EXEC_AND_WAIT_FOR_CALL(launcher().LaunchApp(), observer(), OnAppLaunched());

  EXPECT_FALSE(IsAppInstalledAsPlaceholder());

  // App isn't always ready by the time it's being launched. Therefore we
  // check the total count of kLaunchAppReadinessUMA instead of individual
  // cases.
  histogram.ExpectTotalCount(
      chromeos::KioskAppServiceLauncher::kLaunchAppReadinessUMA, 1);
  histogram.ExpectUniqueSample(
      WebKioskAppServiceLauncher::kWebAppInstallResultUMA,
      webapps::InstallResultCode::kSuccessNewInstall, 1);
}

TEST_F(WebKioskAppServiceLauncherTest, FullFlowAlreadyInstalled) {
  base::HistogramTester histogram;

  InstallApp();

  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), observer(), OnAppPrepared());
  EXEC_AND_WAIT_FOR_CALL(launcher().LaunchApp(), observer(), OnAppLaunched());

  // App isn't always ready by the time it's being launched. Therefore we
  // check the total count of kLaunchAppReadinessUMA instead of individual
  // cases.
  histogram.ExpectTotalCount(
      chromeos::KioskAppServiceLauncher::kLaunchAppReadinessUMA, 1);
  histogram.ExpectTotalCount(
      WebKioskAppServiceLauncher::kWebAppInstallResultUMA, 0);
}

TEST_F(WebKioskAppServiceLauncherTest, FullFlowPlaceholderReplaced) {
  base::HistogramTester histogram;

  InstallAppAsPlaceholder();

  CreateWebAppWithManifest();
  EXEC_AND_WAIT_FOR_CALL(launcher().Initialize(), delegate(),
                         InitializeNetwork());
  EXEC_AND_WAIT_FOR_CALL(launcher().ContinueWithNetworkReady(), observer(),
                         OnAppPrepared());
  EXEC_AND_WAIT_FOR_CALL(launcher().LaunchApp(), observer(), OnAppLaunched());

  EXPECT_FALSE(IsAppInstalledAsPlaceholder());

  // App isn't always ready by the time it's being launched. Therefore we
  // check the total count of kLaunchAppReadinessUMA instead of individual
  // cases.
  histogram.ExpectTotalCount(
      chromeos::KioskAppServiceLauncher::kLaunchAppReadinessUMA, 1);
  histogram.ExpectUniqueSample(
      WebKioskAppServiceLauncher::kWebAppInstallResultUMA,
      webapps::InstallResultCode::kSuccessNewInstall, 1);
  histogram.ExpectUniqueSample(
      WebKioskAppServiceLauncher::kWebAppIsPlaceholderUMA, true, 1);
}

}  // namespace ash
