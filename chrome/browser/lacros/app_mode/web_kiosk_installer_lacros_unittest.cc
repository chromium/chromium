// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/lacros/app_mode/web_kiosk_installer_lacros.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::test::TestFuture;
using WebKioskInstallState = crosapi::mojom::WebKioskInstallState;

namespace {

const char kAppInstallUrl[] = "https://example.com";
const char kAppLaunchUrl[] = "https://example.com/launch";
const char kManifestUrl[] = "https://example.com/manifest.json";
const char16_t kAppTitle[] = u"app-title";

absl::optional<web_app::AppId> app_id() {
  return web_app::GenerateAppId(/*manifest_id=*/absl::nullopt,
                                GURL(kAppLaunchUrl));
}

class FakeWebKioskService : public crosapi::mojom::WebKioskService {
 public:
  using WebKioskInstallState = crosapi::mojom::WebKioskInstallState;
  using GetWebKioskInstallStateCallback =
      crosapi::mojom::WebKioskInstaller::GetWebKioskInstallStateCallback;
  using InstallWebKioskCallback =
      crosapi::mojom::WebKioskInstaller::InstallWebKioskCallback;

  FakeWebKioskService() = default;
  ~FakeWebKioskService() override = default;

  mojo::PendingRemote<crosapi::mojom::WebKioskService>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void BindInstaller(mojo::PendingRemote<crosapi::mojom::WebKioskInstaller>
                         installer) override {
    installer_.Bind(std::move(installer));
    bound_future_.SetValue();
  }

  void WaitUntilBound() { ASSERT_TRUE(bound_future_.Wait()); }

  std::tuple<WebKioskInstallState, absl::optional<web_app::AppId>>
  GetWebKioskInstallState(const GURL& url) {
    TestFuture<WebKioskInstallState, const absl::optional<web_app::AppId>&>
        future;

    installer_->GetWebKioskInstallState(url, future.GetCallback());

    return future.Get();
  }

  absl::optional<web_app::AppId> InstallWebKiosk(const GURL& url) {
    TestFuture<const absl::optional<web_app::AppId>&> future;
    installer_->InstallWebKiosk(url, future.GetCallback());
    return future.Get();
  }

 private:
  TestFuture<void> bound_future_;
  mojo::Receiver<crosapi::mojom::WebKioskService> receiver_{this};
  mojo::Remote<crosapi::mojom::WebKioskInstaller> installer_;
};

}  // namespace

class WebKioskInstallerLacrosTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile(
        "Default", /*is_main_profile=*/true);

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_);
  }

 protected:
  TestingProfile& profile() { return *profile_; }
  FakeWebKioskService& web_kiosk_service() { return web_kiosk_service_; }

  void CreateInstallerAndWaitUntilBound() {
    BindFakeWebKioskService();
    installer_ = std::make_unique<WebKioskInstallerLacros>(profile());
    web_kiosk_service().WaitUntilBound();
  }

  void InstallAppAsPlaceholder() {
    InstallAppInternal(/*install_app_as_placeholder=*/true);
  }

  void InstallApp() {
    CreateWebAppWithManifest();
    InstallAppInternal(/*install_app_as_placeholder=*/false);

    web_app::WebAppInstallInfo info;
    info.start_url = GURL(kAppLaunchUrl);
    info.title = kAppTitle;
  }

  web_app::AppId CreateWebAppWithManifest() {
    const GURL install_url = GURL(kAppInstallUrl);
    const GURL manifest_url = GURL(kManifestUrl);
    const GURL start_url = GURL(kAppLaunchUrl);

    auto& install_page_state =
        web_contents_manager().GetOrCreatePageState(install_url);
    install_page_state.url_load_result =
        web_app::WebAppUrlLoaderResult::kUrlLoaded;
    install_page_state.redirection_url = absl::nullopt;

    install_page_state.opt_metadata =
        web_app::FakeWebContentsManager::CreateMetadataWithTitle(
            u"Basic app title");

    install_page_state.manifest_url = manifest_url;
    install_page_state.valid_manifest_for_web_app = true;

    install_page_state.opt_manifest = blink::mojom::Manifest::New();
    install_page_state.opt_manifest->scope =
        url::Origin::Create(start_url).GetURL();
    install_page_state.opt_manifest->start_url = start_url;
    install_page_state.opt_manifest->id =
        web_app::GenerateManifestIdFromStartUrlOnly(start_url);
    install_page_state.opt_manifest->display =
        blink::mojom::DisplayMode::kStandalone;
    install_page_state.opt_manifest->short_name = u"Basic app name";

    return web_app::GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);
  }

  bool IsAppInstalledAsPlaceholder() {
    return web_app_provider()
        .registrar_unsafe()
        .LookupPlaceholderAppId(GURL(kAppInstallUrl),
                                web_app::WebAppManagement::Type::kKiosk)
        .has_value();
  }

 private:
  void BindFakeWebKioskService() {
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        web_kiosk_service_.BindNewPipeAndPassRemote());
  }

  web_app::WebAppProvider& web_app_provider() {
    return *web_app::WebAppProvider::GetForTest(profile_);
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

  content::BrowserTaskEnvironment task_environment_;

  apps::AppServiceTest app_service_test_;

  FakeWebKioskService web_kiosk_service_;
  std::unique_ptr<WebKioskInstallerLacros> installer_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
};

TEST_F(WebKioskInstallerLacrosTest, CreatingUnboundInstallerShouldNotCrash) {
  // We're not calling CreateInstallerAndWaitUntilBound to construct an unbound
  // installer
  auto installer = std::make_unique<WebKioskInstallerLacros>(profile());
}

TEST_F(WebKioskInstallerLacrosTest, GetInstallStateShouldWorkForNotInstalled) {
  CreateInstallerAndWaitUntilBound();

  EXPECT_EQ(
      web_kiosk_service().GetWebKioskInstallState(GURL(kAppInstallUrl)),
      std::make_tuple(WebKioskInstallState::kNotInstalled, absl::nullopt));
}

TEST_F(WebKioskInstallerLacrosTest,
       GetInstallStateShouldWorkForPlaceholderInstall) {
  CreateInstallerAndWaitUntilBound();
  InstallAppAsPlaceholder();

  EXPECT_EQ(web_kiosk_service().GetWebKioskInstallState(GURL(kAppInstallUrl)),
            std::make_tuple(WebKioskInstallState::kPlaceholderInstalled,
                            absl::nullopt));
}

TEST_F(WebKioskInstallerLacrosTest, GetInstallStateShouldWorkForInstalled) {
  CreateInstallerAndWaitUntilBound();
  InstallApp();

  EXPECT_EQ(web_kiosk_service().GetWebKioskInstallState(GURL(kAppInstallUrl)),
            std::make_tuple(WebKioskInstallState::kInstalled, app_id()));
}

TEST_F(WebKioskInstallerLacrosTest, InstallAppShouldFullyInstallApp) {
  CreateInstallerAndWaitUntilBound();
  CreateWebAppWithManifest();

  auto result_app_id =
      web_kiosk_service().InstallWebKiosk(GURL(kAppInstallUrl));

  EXPECT_EQ(result_app_id, app_id());
  EXPECT_FALSE(IsAppInstalledAsPlaceholder());
}

TEST_F(WebKioskInstallerLacrosTest, InstallAppShouldUpgradePlaceholder) {
  CreateInstallerAndWaitUntilBound();
  InstallAppAsPlaceholder();
  ASSERT_TRUE(IsAppInstalledAsPlaceholder());

  CreateWebAppWithManifest();

  auto result_app_id =
      web_kiosk_service().InstallWebKiosk(GURL(kAppInstallUrl));

  EXPECT_EQ(result_app_id, app_id());
  EXPECT_FALSE(IsAppInstalledAsPlaceholder());
}

TEST_F(WebKioskInstallerLacrosTest,
       InstallAppShouldCreatePlaceholderIfAppDoesntExist) {
  CreateInstallerAndWaitUntilBound();

  auto result_app_id =
      web_kiosk_service().InstallWebKiosk(GURL(kAppInstallUrl));

  EXPECT_TRUE(result_app_id.has_value());
  EXPECT_TRUE(IsAppInstalledAsPlaceholder());
}
