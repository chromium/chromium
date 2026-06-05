// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#endif

using content::WebContents;
namespace {

class CaptureHandleObserver : public content::WebContentsObserver {
 public:
  CaptureHandleObserver(content::WebContents* web_contents,
                        const std::string& expected_handle)
      : content::WebContentsObserver(web_contents),
        expected_handle_(base::UTF8ToUTF16(expected_handle)) {}

  void OnCaptureHandleConfigUpdate(
      const blink::mojom::CaptureHandleConfig& config) override {
    if (config.capture_handle == expected_handle_) {
      run_loop_.Quit();
    }
  }

  void Wait() {
    if (web_contents()
            ->GetPrimaryPage()
            .GetCaptureHandleConfig()
            .capture_handle == expected_handle_) {
      return;
    }
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  std::u16string expected_handle_;
};

class WindowCaptureSession {
 public:
  WindowCaptureSession(Browser* target_b, Browser* capturer_b)
      : target_browser_(target_b), capturer_browser_(capturer_b) {
    target_contents_ =
        target_browser_->tab_strip_model()->GetActiveWebContents();
  }

  testing::AssertionResult SetCaptureHandleConfig(
      const std::string& handle_to_set,
      bool expose_origin = true,
      const std::vector<std::string>& permitted_origins = {"*"}) {
    base::ListValue origins_list;
    for (const auto& origin : permitted_origins) {
      origins_list.Append(origin);
    }

    CaptureHandleObserver observer(target_contents_, handle_to_set);

    std::string js_result =
        content::EvalJs(target_contents_,
                        content::JsReplace(R"(
    try {
      navigator.mediaDevices.setCaptureHandleConfig({
        handle: $1, exposeOrigin: $2, permittedOrigins: $3
      });
      'success';
    } catch (e) {
      e.message;
    }
  )",
                                           handle_to_set, expose_origin,
                                           std::move(origins_list)))
            .ExtractString();

    if (js_result != "success") {
      return testing::AssertionFailure()
             << "Failed to set capture handle in JS. Error: " << js_result;
    }

    observer.Wait();
    return testing::AssertionSuccess();
  }

  std::string GetExpectedJson(bool expose_origin, const std::string& handle) {
    if (!expose_origin && handle.empty()) {
      return "";
    }
    base::DictValue dict;
    dict.Set("handle", handle);

    if (expose_origin) {
      const auto origin = target_contents_->GetPrimaryPage()
                              .GetMainDocument()
                              .GetLastCommittedOrigin();
      dict.Set("origin", origin.Serialize());
    }

    std::string expected_json;
    base::JSONWriter::Write(dict, &expected_json);

    return expected_json;
  }

  void StartCapturing(net::EmbeddedTestServer* test_server) {
    GURL capturer_url = test_server->GetURL("/webrtc/capturing_page_main.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(capturer_browser_, capturer_url));
    capturer_contents_ =
        capturer_browser_->tab_strip_model()->GetActiveWebContents();
    permissions::PermissionRequestManager::FromWebContents(capturer_contents_)
        ->set_auto_response_for_test(
            permissions::PermissionRequestManager::ACCEPT_ALL);

    // This forces both tabs to pump video frames, preventing WebRTC timeouts.
    target_browser_->window()->Show();
    target_contents_->WasShown();
    capturer_browser_->window()->Show();
    capturer_contents_->WasShown();
    EXPECT_EQ("capture-success",
              content::EvalJs(capturer_contents_, "captureOtherTab();"));
  }

  std::string ReadCaptureHandle() {
    return content::EvalJs(capturer_contents_, "readCaptureHandle();")
        .ExtractString();
  }

  std::string ReadLastEvent() {
    return content::EvalJs(capturer_contents_, "readLastEvent();")
        .ExtractString();
  }

  void NavigateTargetWithinSameDocumentAndWait(const std::string& new_route) {
    content::TestNavigationObserver nav_observer(target_contents_);
    ASSERT_TRUE(content::ExecJs(
        target_contents_,
        "window.history.pushState({}, '', '" + new_route + "');"));
    nav_observer.Wait();
  }

  void AddIframe(const std::string& iframe_id) {
    ASSERT_TRUE(
        content::ExecJs(target_contents_,
                        "let iframe = document.createElement('iframe'); "
                        "iframe.id = '" +
                            iframe_id +
                            "'; "
                            "document.body.appendChild(iframe);"));
  }

  void NavigateTargetCrossDocumentAndWait(const std::string& new_route) {
    content::TestNavigationObserver nav_observer(target_contents_);
    ASSERT_TRUE(content::ExecJs(target_contents_,
                                "window.location.href = '" + new_route + "';"));
    nav_observer.Wait();
  }

  void ReloadTargetAndWait() {
    content::TestNavigationObserver reload_observer(target_contents_);
    target_contents_->GetController().Reload(content::ReloadType::NORMAL,
                                             /*check_for_repost=*/false);
    reload_observer.Wait();
  }

  Browser* target_browser() const { return target_browser_; }
  content::WebContents* target_contents() const { return target_contents_; }
  Browser* capturer_browser() const { return capturer_browser_; }
  content::WebContents* capturer_contents() const { return capturer_contents_; }

 private:
  raw_ptr<Browser> target_browser_ = nullptr;
  raw_ptr<content::WebContents> target_contents_ = nullptr;
  raw_ptr<Browser> capturer_browser_ = nullptr;
  raw_ptr<content::WebContents> capturer_contents_ = nullptr;
};

const std::string& GetCapturedWindowTitle() {
  // Because this is static, it is initialized exactly once per OS process
  // (batch). All tests in this batch will share it, while parallel batches
  // get unique ones.
  static const std::string title =
      base::StringPrintf("Capture Target - %s",
                         base::UnguessableToken::Create().ToString().c_str());
  return title;
}

void SetTitleAndWait(content::WebContents* web_contents) {
  std::u16string expected_title = base::UTF8ToUTF16(GetCapturedWindowTitle());
  content::TitleWatcher title_watcher(web_contents, expected_title);
  ASSERT_TRUE(content::ExecJs(
      web_contents, "document.title = '" + GetCapturedWindowTitle() + "';"));
  ASSERT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
}

void SetTitleToClosedAndWait(Browser* browser) {
  if (auto* contents = browser->tab_strip_model()->GetActiveWebContents()) {
    content::TitleWatcher title_watcher(contents, u"Closed");
    EXPECT_TRUE(content::ExecJs(contents, "document.title = 'Closed';"));
    EXPECT_EQ(u"Closed", title_watcher.WaitAndGetTitle());
  }
}

}  // namespace

class CaptureHandleWindowBrowserTest : public WebRtcTestBase {
 public:
  CaptureHandleWindowBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kCaptureHandleForStandalonePwasAndIwas,
         blink::features::kDesktopPWAsTabStrip,
         features::kDesktopPWAsTabStripSettings},
        /*disabled_features=*/{});
  }
  ~CaptureHandleWindowBrowserTest() override = default;

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();
    os_integration_override_ = std::make_unique<
        web_app::OsIntegrationTestOverrideBlockingRegistration>();
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    if (session_) {
      session_.reset();
    }
    os_integration_override_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SafeCloseBrowser(Browser* browser) {
    if (session_) {
      session_.reset();
    }
    SetTitleToClosedAndWait(browser);
    CloseBrowserSynchronously(browser);
  }

  void SetTitleClosedAndUninstallApp(Browser* app_browser,
                                     const webapps::AppId& app_id) {
    if (session_) {
      session_.reset();
    }
    SetTitleToClosedAndWait(app_browser);
    web_app::test::UninstallWebApp(browser()->profile(), app_id);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectWindowCaptureSourceByTitle,
        GetCapturedWindowTitle());
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
#if !BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitch(switches::kUseGpuInTests);
#endif
  }

  std::unique_ptr<WindowCaptureSession> session_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<web_app::OsIntegrationTestOverrideBlockingRegistration>
      os_integration_override_;
};

IN_PROC_BROWSER_TEST_F(CaptureHandleWindowBrowserTest,
                       IgnoresHandleFromRegularBrowserWindow) {
  Browser* target_browser = CreateBrowser(browser()->profile());
  base::ScopedClosureRunner auto_close_target(
      base::BindOnce(&CaptureHandleWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), target_browser));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      target_browser, embedded_test_server()->GetURL("/empty.html")));
  session_ = std::make_unique<WindowCaptureSession>(target_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig("regular-handle"));
  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ("null", session_->ReadCaptureHandle());
}

IN_PROC_BROWSER_TEST_F(CaptureHandleWindowBrowserTest,
                       IgnoresHandleFromDiyAppWindow) {
  GURL diy_app_url = embedded_test_server()->GetURL("/simple.html");
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(diy_app_url);
  web_app_info->title = base::UTF8ToUTF16(GetCapturedWindowTitle());
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info->is_diy_app = true;
  webapps::AppId diy_app_id = web_app::test::InstallWebApp(
      browser()->profile(), std::move(web_app_info));

  Browser* target_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), diy_app_id);
  ASSERT_TRUE(target_browser);
  base::ScopedClosureRunner auto_close_target(base::BindOnce(
      &CaptureHandleWindowBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), target_browser, diy_app_id));

  session_ = std::make_unique<WindowCaptureSession>(target_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig("diy-handle"));
  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ("null", session_->ReadCaptureHandle());
}

class CaptureHandlePlaceholderBrowserTest
    : public CaptureHandleWindowBrowserTest,
      public testing::WithParamInterface<web_app::WebAppManagement::Type> {
 public:
  CaptureHandlePlaceholderBrowserTest() = default;
  ~CaptureHandlePlaceholderBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_P(CaptureHandlePlaceholderBrowserTest,
                       IgnoresHandleFromPlaceholder) {
  const std::string expected_handle = "placeholder-handle";
  GURL app_url =
      embedded_test_server()->GetURL("/this-page-does-not-exist.html");
  web_app::WebAppManagement::Type management_type = GetParam();

  web_app::ExternalInstallSource install_source =
      (management_type == web_app::WebAppManagement::Type::kKiosk)
          ? web_app::ExternalInstallSource::kKiosk
          : web_app::ExternalInstallSource::kExternalPolicy;

  web_app::ExternalInstallOptions install_options(
      app_url, web_app::mojom::UserDisplayMode::kStandalone, install_source);

  install_options.install_placeholder = true;
  install_options.fallback_app_name = GetCapturedWindowTitle();

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(browser()->profile());
  ASSERT_TRUE(provider);

  base::test::TestFuture<web_app::ExternallyManagedAppManager::InstallResult>
      future;
  provider->scheduler().InstallExternallyManagedApp(
      install_options,
      /*installed_placeholder_app_id=*/std::nullopt, future.GetCallback());

  const web_app::ExternallyManagedAppManager::InstallResult& result =
      future.Get<web_app::ExternallyManagedAppManager::InstallResult>();

  ASSERT_TRUE(result.app_id.has_value())
      << "Installation completely failed with code: "
      << base::ToString(result.code);

  webapps::AppId app_id = *result.app_id;
  EXPECT_TRUE(
      provider->registrar_unsafe().IsPlaceholderApp(app_id, management_type));

  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);
  ASSERT_TRUE(app_browser);
  base::ScopedClosureRunner auto_close_target(
      base::BindOnce(&CaptureHandlePlaceholderBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), app_browser));

  content::WebContents* app_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(
      app_contents, embedded_test_server()->GetURL("/empty.html")));

  session_ = std::make_unique<WindowCaptureSession>(app_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(session_->ReadCaptureHandle(), "null");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CaptureHandlePlaceholderBrowserTest,
    testing::Values(web_app::WebAppManagement::Type::kKiosk,
                    web_app::WebAppManagement::Type::kPolicy));

class CaptureHandlePwaBrowserTest : public CaptureHandleWindowBrowserTest {
 public:
  CaptureHandlePwaBrowserTest() = default;
  ~CaptureHandlePwaBrowserTest() override = default;

 protected:
  webapps::AppId InstallTabbedPWA(Profile* profile, const GURL& start_url) {
    blink::Manifest::TabStrip tab_strip;
    tab_strip.home_tab = blink::Manifest::HomeTabParams();
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->display_override = {
        web_app::DisplayOverride::Create(blink::mojom::DisplayMode::kTabbed)};
    web_app_info->title = u"A Web App";
    web_app_info->tab_strip = std::move(tab_strip);
    return web_app::test::InstallWebApp(profile, std::move(web_app_info));
  }

  webapps::AppId InstallStandalonePWA(Profile* profile, const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app_info->title = u"A Web App";
    return web_app::test::InstallWebApp(profile, std::move(web_app_info));
  }

  const std::string expected_handle_ = "pwa-handle";
};

IN_PROC_BROWSER_TEST_F(CaptureHandlePwaBrowserTest,
                       ExtractsHandleFromPwaWindow) {
  GURL pwa_url = embedded_test_server()->GetURL("/title1.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallStandalonePWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);
  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));
}

IN_PROC_BROWSER_TEST_F(CaptureHandlePwaBrowserTest,
                       HandlePersistsOnSameDocumentNavigation) {
  GURL pwa_url = embedded_test_server()->GetURL("/title2.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallStandalonePWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);
  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));
  session_->NavigateTargetWithinSameDocumentAndWait("/new-route.html");
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));
}

IN_PROC_BROWSER_TEST_F(CaptureHandlePwaBrowserTest, IgnoresTabbedPwaWindows) {
  GURL pwa_url = embedded_test_server()->GetURL("/title3.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallTabbedPWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);

  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(session_->ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandlePwaBrowserTest,
    CrossDocumentChildPageNavigationDoesNotClearCaptureHandleConfig) {
  GURL pwa_url = embedded_test_server()->GetURL("/title1.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallStandalonePWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);
  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());
  session_->AddIframe("test_subframe");

  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  GURL child_url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(content::NavigateIframeToURL(session_->target_contents(),
                                           "test_subframe", child_url));

  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));
}

IN_PROC_BROWSER_TEST_F(CaptureHandlePwaBrowserTest, RespectsExposeOriginFalse) {
  GURL pwa_url = embedded_test_server()->GetURL("/title1.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallStandalonePWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);
  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());

  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_,
                                               /*expose_origin=*/false,
                                               /*permitted_origins=*/{"*"}));

  session_->StartCapturing(embedded_test_server());

  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/false, expected_handle_));
}

IN_PROC_BROWSER_TEST_F(CaptureHandlePwaBrowserTest, RespectsPermittedOrigins) {
  GURL pwa_url = embedded_test_server()->GetURL("/title1.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallStandalonePWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);
  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());

  ASSERT_TRUE(session_->SetCaptureHandleConfig(
      expected_handle_,
      /*expose_origin=*/true, {"https://unauthorized-origin.com"}));
  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(session_->ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(CaptureHandlePwaBrowserTest,
                       HandlePushesDynamicUpdatesToCapturer) {
  GURL pwa_url = embedded_test_server()->GetURL("/title1.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallStandalonePWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);

  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  const std::string new_dynamic_handle = "new-dynamic-handle";
  ASSERT_TRUE(session_->SetCaptureHandleConfig(new_dynamic_handle));

  EXPECT_EQ(
      session_->ReadLastEvent(),
      session_->GetExpectedJson(/*expose_origin=*/true, new_dynamic_handle));
}

IN_PROC_BROWSER_TEST_F(CaptureHandlePwaBrowserTest,
                       HandleClearPushedToCapturerOnPageReload) {
  GURL pwa_url = embedded_test_server()->GetURL("/title1.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallStandalonePWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);

  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  session_->ReloadTargetAndWait();
  EXPECT_EQ(session_->ReadLastEvent(), "null");
}

IN_PROC_BROWSER_TEST_F(CaptureHandlePwaBrowserTest,
                       HandleDynamicallyClearedOnNavigation) {
  GURL pwa_url = embedded_test_server()->GetURL("/title2.html");
  Profile* profile = browser()->profile();
  webapps::AppId pwa_id = InstallStandalonePWA(profile, pwa_url);
  Browser* pwa_browser = web_app::LaunchWebAppBrowserAndWait(profile, pwa_id);
  ASSERT_TRUE(pwa_browser);

  base::ScopedClosureRunner auto_close_pwa(base::BindOnce(
      &CaptureHandlePwaBrowserTest::SetTitleClosedAndUninstallApp,
      base::Unretained(this), pwa_browser, pwa_id));

  session_ = std::make_unique<WindowCaptureSession>(pwa_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  session_->NavigateTargetCrossDocumentAndWait("/title3.html");
  EXPECT_EQ(session_->ReadLastEvent(), "null");
}

class CaptureHandleIwaWindowBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  CaptureHandleIwaWindowBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kCaptureHandleForStandalonePwasAndIwas},
        /*disabled_features=*/{});
  }

  void TearDownOnMainThread() override {
    if (session_) {
      session_.reset();
    }
    web_app::IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  void SafeCloseBrowser(Browser* browser) {
    if (session_) {
      session_.reset();
    }
    SetTitleToClosedAndWait(browser);
    CloseBrowserSynchronously(browser);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectWindowCaptureSourceByTitle,
        GetCapturedWindowTitle());
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
#if !BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitch(switches::kUseGpuInTests);
#endif
  }

  void SetUpOnMainThread() override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  base::expected<web_app::IsolatedWebAppUrlInfo, std::string> InstallIwa() {
    auto app =
        web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                           .SetName(GetCapturedWindowTitle())
                                           .SetVersion("1.0.0"))
            .AddHtml("/",
                     "<head><title>" + GetCapturedWindowTitle() +
                         "</title></head><body>App<iframe id=\"test_subframe\" "
                         "name=\"test_subframe\" src=\"/empty.html\"></iframe>"
                         "</body>")
            .AddHtml("/empty.html", "<body>Empty Starting Page</body>")
            .AddHtml("/child.html",
                     "<head><title>Child</title></head><body>Child Destination "
                     "Page</body>")
            .BuildBundle();
    app->TrustSigningKey();
    return app->Install(profile());
  }

  const std::string expected_handle_ = "iwa-handle";
  std::unique_ptr<WindowCaptureSession> session_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CaptureHandleIwaWindowBrowserTest,
                       ExtractsHandleFromIWA) {
  auto url_info = InstallIwa();
  ASSERT_TRUE(url_info.has_value());
  Browser* iwa_browser =
      web_app::LaunchWebAppBrowserAndWait(profile(), url_info->app_id());
  EXPECT_TRUE(iwa_browser);
  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleIwaWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), iwa_browser));

  session_ = std::make_unique<WindowCaptureSession>(iwa_browser, browser());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));
}

IN_PROC_BROWSER_TEST_F(CaptureHandleIwaWindowBrowserTest,
                       HandlePersistsOnSameDocumentNavigation) {
  auto url_info = InstallIwa();
  ASSERT_TRUE(url_info.has_value());
  Browser* iwa_browser =
      web_app::LaunchWebAppBrowserAndWait(profile(), url_info->app_id());
  EXPECT_TRUE(iwa_browser);
  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleIwaWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), iwa_browser));

  session_ = std::make_unique<WindowCaptureSession>(iwa_browser, browser());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  session_->NavigateTargetWithinSameDocumentAndWait("/new-route.html");
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleIwaWindowBrowserTest,
    CrossDocumentChildPageNavigationDoesNotClearCaptureHandleConfig) {
  auto url_info = InstallIwa();
  ASSERT_TRUE(url_info.has_value());
  Browser* iwa_browser =
      web_app::LaunchWebAppBrowserAndWait(profile(), url_info->app_id());
  EXPECT_TRUE(iwa_browser);
  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleIwaWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), iwa_browser));

  session_ = std::make_unique<WindowCaptureSession>(iwa_browser, browser());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  GURL child_url = url_info->origin().GetURL().Resolve("/child.html");
  content::TestNavigationObserver nav_observer(session_->target_contents());

  ASSERT_TRUE(content::ExecJs(
      session_->target_contents(),
      "document.getElementById('test_subframe').src = '/child.html';"));
  nav_observer.Wait();

  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(nav_observer.last_navigation_url(), child_url);

  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));
}

IN_PROC_BROWSER_TEST_F(CaptureHandleIwaWindowBrowserTest,
                       RespectsExposeOriginFalse) {
  auto url_info = InstallIwa();
  ASSERT_TRUE(url_info.has_value());
  Browser* iwa_browser =
      web_app::LaunchWebAppBrowserAndWait(profile(), url_info->app_id());
  EXPECT_TRUE(iwa_browser);
  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleIwaWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), iwa_browser));

  session_ = std::make_unique<WindowCaptureSession>(iwa_browser, browser());

  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_,
                                               /*expose_origin=*/false,
                                               /*permitted_origins=*/{"*"}));

  session_->StartCapturing(embedded_test_server());

  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/false, expected_handle_));
}

IN_PROC_BROWSER_TEST_F(CaptureHandleIwaWindowBrowserTest,
                       RespectsPermittedOrigins_Unauthorized) {
  auto url_info = InstallIwa();
  ASSERT_TRUE(url_info.has_value());
  Browser* iwa_browser =
      web_app::LaunchWebAppBrowserAndWait(profile(), url_info->app_id());
  EXPECT_TRUE(iwa_browser);
  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleIwaWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), iwa_browser));

  session_ = std::make_unique<WindowCaptureSession>(iwa_browser, browser());

  ASSERT_TRUE(session_->SetCaptureHandleConfig(
      expected_handle_,
      /*expose_origin=*/true,
      /*permitted_origins=*/{"https://unauthorized-origin.com"}));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(session_->ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(CaptureHandleIwaWindowBrowserTest,
                       HandlePushesDynamicUpdatesToCapturer) {
  auto url_info = InstallIwa();
  ASSERT_TRUE(url_info.has_value());
  Browser* iwa_browser =
      web_app::LaunchWebAppBrowserAndWait(profile(), url_info->app_id());
  EXPECT_TRUE(iwa_browser);

  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleIwaWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), iwa_browser));

  session_ = std::make_unique<WindowCaptureSession>(iwa_browser, browser());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  const std::string new_dynamic_handle = "new-dynamic-handle";
  ASSERT_TRUE(session_->SetCaptureHandleConfig(new_dynamic_handle));
  EXPECT_EQ(
      session_->ReadLastEvent(),
      session_->GetExpectedJson(/*expose_origin=*/true, new_dynamic_handle));
}

IN_PROC_BROWSER_TEST_F(CaptureHandleIwaWindowBrowserTest,
                       HandleClearPushedToCapturerOnPageReload) {
  auto url_info = InstallIwa();
  ASSERT_TRUE(url_info.has_value());
  Browser* iwa_browser =
      web_app::LaunchWebAppBrowserAndWait(profile(), url_info->app_id());
  EXPECT_TRUE(iwa_browser);

  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleIwaWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), iwa_browser));

  session_ = std::make_unique<WindowCaptureSession>(iwa_browser, browser());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  session_->ReloadTargetAndWait();
  EXPECT_EQ(session_->ReadLastEvent(), "null");
}

IN_PROC_BROWSER_TEST_F(CaptureHandleIwaWindowBrowserTest,
                       HandleDynamicallyClearedOnNavigation) {
  auto url_info = InstallIwa();
  ASSERT_TRUE(url_info.has_value());
  Browser* iwa_browser =
      web_app::LaunchWebAppBrowserAndWait(profile(), url_info->app_id());
  EXPECT_TRUE(iwa_browser);

  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleIwaWindowBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), iwa_browser));

  session_ = std::make_unique<WindowCaptureSession>(iwa_browser, browser());
  ASSERT_TRUE(session_->SetCaptureHandleConfig(expected_handle_));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(
      session_->ReadCaptureHandle(),
      session_->GetExpectedJson(/*expose_origin=*/true, expected_handle_));

  session_->NavigateTargetCrossDocumentAndWait("/title3.html");
  EXPECT_EQ(session_->ReadLastEvent(), "null");
}

#if BUILDFLAG(IS_CHROMEOS)
class CaptureHandleSystemWebAppBrowserTest
    : public ash::SystemWebAppBrowserTestBase {
 public:
  CaptureHandleSystemWebAppBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kCaptureHandleForStandalonePwasAndIwas},
        /*disabled_features=*/{});
  }
  ~CaptureHandleSystemWebAppBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ash::SystemWebAppBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    if (session_) {
      session_.reset();
    }
    ash::SystemWebAppBrowserTestBase::TearDownOnMainThread();
  }

  void SafeCloseBrowser(Browser* browser) {
    if (session_) {
      session_.reset();
    }
    SetTitleToClosedAndWait(browser);
    CloseBrowserSynchronously(browser);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ash::SystemWebAppBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectWindowCaptureSourceByTitle,
        GetCapturedWindowTitle());
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

 protected:
  std::unique_ptr<WindowCaptureSession> session_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/520451851): Test is increasingly timing out more often.
// Re-enable once time out issue is addressed. Likely caused by the
// SetTitleAndWait call.
IN_PROC_BROWSER_TEST_F(CaptureHandleSystemWebAppBrowserTest,
                       DISABLED_IgnoresHandleFromSystemWebApp) {
  WaitForTestSystemAppInstall();
  content::WebContents* swa_contents =
      LaunchApp(ash::SystemWebAppType::SETTINGS);
  ASSERT_TRUE(swa_contents);
  EXPECT_TRUE(content::WaitForLoadStop(swa_contents));

  Browser* swa_browser = ash::FindSystemWebAppBrowser(
      browser()->profile(), ash::SystemWebAppType::SETTINGS);
  ASSERT_TRUE(swa_browser);

  base::ScopedClosureRunner auto_close(
      base::BindOnce(&CaptureHandleSystemWebAppBrowserTest::SafeCloseBrowser,
                     base::Unretained(this), swa_browser));

  session_ = std::make_unique<WindowCaptureSession>(swa_browser, browser());
  SetTitleAndWait(session_->target_contents());
  ASSERT_TRUE(session_->SetCaptureHandleConfig("swa-handle"));

  session_->StartCapturing(embedded_test_server());
  EXPECT_EQ(session_->ReadCaptureHandle(), "null");
}
#endif  // BUILDFLAG(IS_CHROMEOS)
