// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/office_web_app/office_web_app.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace chromeos {
namespace {

// This URL will result in the unexpected app ID.
constexpr char kUnexpectedWebAppUrl[] = "https://www.microsoft365.com/?auth=1";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://www.microsoft365.com/?auth=1"))
constexpr char kUnexpectedMicrosoft365AppId[] =
    "ioolobdpeeccbgnljglmgfmbgboajhdf";

using base::test::TestFuture;

// Test class to check that the Office (Microsoft365) web app can be installed
// online and offline.
class OfficeWebAppUnitTest : public WebAppTest {
 protected:
  OfficeWebAppUnitTest() = default;
  ~OfficeWebAppUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  // Make `LoadUrl` to return `kUrlLoaded` for the specified URL.
  void SetupUrlLoadSuccess(const GURL& url) {
    auto& web_contents_manager = static_cast<web_app::FakeWebContentsManager&>(
        web_app::WebAppProvider::GetForTest(profile())->web_contents_manager());
    auto& fake_page_state = web_contents_manager.GetOrCreatePageState(url);
    fake_page_state.url_load_result =
        webapps::WebAppUrlLoaderResult::kUrlLoaded;
  }

  webapps::AppId SetupInstalledMicrosoft365(
      const GURL& install_url,
      const GURL& start_url,
      webapps::WebappInstallSource install_source) {
    auto install_info =
        std::make_unique<web_app::WebAppInstallInfo>(start_url, start_url);
    install_info->install_url = install_url;
    install_info->title = u"Microsoft 365";
    return web_app::test::InstallWebApp(
        profile(), std::move(install_info),
        /*overwrite_existing_manifest_fields=*/false, install_source);
  }

  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(OfficeWebAppUnitTest, InstallMicrosoft365WhenOffline) {
  TestFuture<webapps::InstallResultCode> future;
  InstallMicrosoft365(profile(), future.GetCallback());
  EXPECT_EQ(future.Get(),
            webapps::InstallResultCode::kSuccessOfflineOnlyInstall);
}

TEST_F(OfficeWebAppUnitTest, InstallMicrosoft365WhenOnline) {
  // Set the behaviour of `LoadUrl` to return `kUrlLoaded` for the Microsoft365
  // install URL (set the system to be online).
  SetupUrlLoadSuccess(GURL(kMicrosoft365WebAppUrl));

  TestFuture<webapps::InstallResultCode> future;
  InstallMicrosoft365(profile(), future.GetCallback());
  EXPECT_EQ(future.Get(), webapps::InstallResultCode::kSuccessNewInstall);
}

TEST_F(OfficeWebAppUnitTest, ReplaceUnexpectedMicrosoft365App) {
  auto* provider = web_app::WebAppProvider::GetForTest(profile());
  auto& registrar = provider->registrar_unsafe();

  // Install the app with the unexpected ID (via office setup).
  EXPECT_EQ(SetupInstalledMicrosoft365(
                /*install_url=*/GURL(kMicrosoft365WebAppUrl),
                /*start_url=*/GURL(kUnexpectedWebAppUrl),
                webapps::WebappInstallSource::MICROSOFT_365_SETUP),
            kUnexpectedMicrosoft365AppId);
  EXPECT_TRUE(registrar.IsInRegistrar(kUnexpectedMicrosoft365AppId));

  // Set the behaviour of `LoadUrl` to return `kUrlLoaded` for the Microsoft365
  // install URL (set the system to be online).
  SetupUrlLoadSuccess(GURL(kMicrosoft365WebAppUrl));

  // Install the app.
  web_app::WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListening({kUnexpectedMicrosoft365AppId});
  TestFuture<webapps::InstallResultCode> future;
  InstallMicrosoft365(profile(), future.GetCallback());

  EXPECT_EQ(future.Get(), webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(registrar.IsInRegistrar(ash::kMicrosoft365AppId));
  uninstall_observer.Wait();
}

TEST_F(OfficeWebAppUnitTest, DoNotReplaceManuallyInstalledMicrosoft365App) {
  auto* provider = web_app::WebAppProvider::GetForTest(profile());
  auto& registrar = provider->registrar_unsafe();

  // Install the app with the unexpected ID (manually).
  EXPECT_EQ(SetupInstalledMicrosoft365(
                /*install_url=*/GURL(kMicrosoft365WebAppUrl),
                /*start_url=*/GURL(kUnexpectedWebAppUrl),
                webapps::WebappInstallSource::MENU_BROWSER_TAB),
            kUnexpectedMicrosoft365AppId);
  EXPECT_TRUE(registrar.IsInRegistrar(kUnexpectedMicrosoft365AppId));

  // Set the behaviour of `LoadUrl` to return `kUrlLoaded` for the Microsoft365
  // install URL (set the system to be online).
  SetupUrlLoadSuccess(GURL(kMicrosoft365WebAppUrl));

  // Install the app.
  TestFuture<webapps::InstallResultCode> future;
  InstallMicrosoft365(profile(), future.GetCallback());
  // Makes sure the uninstall task is NOT run (waiting for the future only waits
  // for install to complete).
  // NOTE: we have to use RunUntilIdle as we are expecting an async uninstall
  // task to NOT happen, so there is no condition to wait on.
  task_environment()->RunUntilIdle();

  EXPECT_EQ(future.Get(), webapps::InstallResultCode::kSuccessNewInstall);
  EXPECT_TRUE(registrar.IsInRegistrar(ash::kMicrosoft365AppId));
  // Manually installed app is not replaced.
  EXPECT_TRUE(registrar.IsInRegistrar(kUnexpectedMicrosoft365AppId));
}

}  // namespace
}  // namespace chromeos
