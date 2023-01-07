// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/app_mode/app_session_ash.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash {

class WebKioskBrowserControllerAshTest : public InProcessBrowserTest,
                                         public BrowserListObserver {
 public:
  WebKioskBrowserControllerAshTest() = default;
  WebKioskBrowserControllerAshTest(const WebKioskBrowserControllerAshTest&) =
      delete;
  WebKioskBrowserControllerAshTest& operator=(
      const WebKioskBrowserControllerAshTest&) = delete;

  ~WebKioskBrowserControllerAshTest() override = default;

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void set_browser_added_callback(base::OnceClosure browser_added_callback) {
    browser_added_callback_ = std::move(browser_added_callback);
  }

  void set_browser_removed_callback(
      base::OnceClosure browser_removed_callback) {
    browser_removed_callback_ = std::move(browser_removed_callback);
  }

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();
    app_service_test_.SetUp(profile);
    web_app::test::WaitUntilReady(web_app::WebAppProvider::GetForTest(profile));
    BrowserList::AddObserver(this);
  }

  void TearDownOnMainThread() override {
    BrowserList::RemoveObserver(this);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    InProcessBrowserTest::SetUp();
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    if (browser_added_callback_) {
      std::move(browser_added_callback_).Run();
    }
  }

  void OnBrowserRemoved(Browser* browser) override {
    if (browser_removed_callback_) {
      std::move(browser_removed_callback_).Run();
    }
  }

 private:
  net::EmbeddedTestServer https_server_;
  apps::AppServiceTest app_service_test_;

  base::OnceClosure browser_added_callback_;
  base::OnceClosure browser_removed_callback_;
};

// Verifies that Kiosk browser window handler is installed in Kiosk session when
// the web app is launched with |WebKioskBrowserControllerAsh|.
IN_PROC_BROWSER_TEST_F(WebKioskBrowserControllerAshTest,
                       WindowHandlerInstalled) {
  ASSERT_TRUE(https_server()->Start());
  const GURL start_url =
      https_server()->GetURL("/banners/manifest_test_page.html");

  LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                      LoginState::LOGGED_IN_USER_KIOSK);

  web_app::AppId app_id;
  {
    std::unique_ptr<WebAppInstallInfo> install_info =
        std::make_unique<WebAppInstallInfo>();
    install_info->start_url = start_url;
    install_info->scope = start_url.GetWithoutFilename();
    install_info->title = u"App Name";
    install_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;

    base::RunLoop run_loop;
    auto* provider = web_app::WebAppProvider::GetForTest(browser()->profile());
    provider->scheduler().InstallFromInfo(
        std::move(install_info),
        /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::KIOSK,
        base::BindLambdaForTesting(
            [&app_id, &run_loop](const web_app::AppId& installed_app_id,
                                 webapps::InstallResultCode code) {
              EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, code);
              app_id = installed_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  EXPECT_FALSE(WebKioskAppManager::Get()->app_session());

  {
    base::RunLoop run_loop;
    apps::AppLaunchParams params(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromKiosk);
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(std::move(params));
    run_loop.RunUntilIdle();
  }

  EXPECT_TRUE(WebKioskAppManager::Get()->app_session());

  // Verify that new regular windows cannot be opened.
  const auto* browser_list = BrowserList::GetInstance();
  {
    base::RunLoop run_loop;
    set_browser_removed_callback(run_loop.QuitClosure());
    auto* new_browser =
        Browser::Create(Browser::CreateParams(browser()->profile(), true));
    NavigateParams nav_params(
        new_browser, start_url,
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
    Navigate(&nav_params);

    EXPECT_TRUE(new_browser);
    EXPECT_TRUE(new_browser->window());
    new_browser->window()->Show();

    // The newly opened browser will be closed and removed from |BrowserList|.
    run_loop.Run();

    EXPECT_FALSE(base::Contains(*browser_list, new_browser));
  }

  // Verify that accessibility settings can be opened as popup.
  {
    base::RunLoop run_loop;
    set_browser_added_callback(run_loop.QuitWhenIdleClosure());
    const auto settings_url = chrome::GetOSSettingsUrl("manageAccessibility");
    NavigateParams nav_params(
        browser()->profile(), settings_url,
        ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
    nav_params.disposition = WindowOpenDisposition::NEW_POPUP;
    Navigate(&nav_params);

    // The newly opened browser will be allowed and stored by |AppSession|.
    run_loop.Run();

    Browser* settings_browser = WebKioskAppManager::Get()
                                    ->app_session()
                                    ->GetSettingsBrowserForTesting();
    EXPECT_TRUE(settings_browser);
    EXPECT_TRUE(base::Contains(*browser_list, settings_browser));
  }
}

}  // namespace ash
