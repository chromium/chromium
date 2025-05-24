// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_window_closer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

namespace {

// This web page opens a PDF file once `link1` is clicked.
// That triggers a mime handler guest view creation.
constexpr std::string_view kPdfOpenerUrl = "/pdf/test-iframe-pdf.html";

void NotifyKioskGuestAdded(content::WebContents* guest_web_contents) {
  KioskController::Get().OnGuestAdded(guest_web_contents);
}

// Creates a new browser with enabled kiosk troubleshooting tools policy,
// navigates to the given `page_url` and returns it's active `WebContents`.
content::WebContents* OpenUrlInBrowser(GURL page_url) {
  // Enable troubleshooting tools to be able to open a browser in kiosk mode.
  ash::kiosk::test::CurrentProfile().GetPrefs()->SetBoolean(
      prefs::kKioskTroubleshootingToolsEnabled, true);

  Browser::CreateParams params =
      Browser::CreateParams(Browser::Type::TYPE_NORMAL,
                            /*profile=*/&ash::kiosk::test::CurrentProfile(),
                            /*user_gesture=*/true);
  auto& new_browser = CHECK_DEREF(Browser::Create(params));
  new_browser.window()->Show();
  ui_test_utils::NavigateToURLWithDisposition(
      &new_browser, page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  return new_browser.tab_strip_model()->GetActiveWebContents();
}

// Open a PDF file in iframe of test-iframe-pdf.html web page.
// `web_contents` is active browser web contents of test-iframe-pdf.html.
bool OpenPdfInWebContents(content::WebContents* web_contents) {
  return content::ExecJs(
      web_contents,
      "var button_to_open_pdf = document.getElementById('link1');"
      "button_to_open_pdf.click();");
}

guest_view::TestGuestViewManager& GetGuestViewManager(
    guest_view::TestGuestViewManagerFactory& factory) {
  return CHECK_DEREF(factory.GetOrCreateTestGuestViewManager(
      &ash::kiosk::test::CurrentProfile(),
      extensions::ExtensionsAPIClient::Get()
          ->CreateGuestViewManagerDelegate()));
}

KioskMixin::DefaultServerWebAppOption PdfWebApp() {
  return KioskMixin::DefaultServerWebAppOption{
      /*account_id=*/"pdf-web-app@localhost",
      /*url_path=*/kPdfOpenerUrl};
}

}  // namespace

class KioskGuestViewTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskGuestViewTest() {
    // Force allow Chrome Apps in Kiosk, since they are default disabled since
    // M138.
    scoped_feature_list_.InitFromCommandLine("AllowChromeAppsInKioskSessions",
                                             "");
  }
  KioskGuestViewTest(const KioskGuestViewTest&) = delete;
  KioskGuestViewTest& operator=(const KioskGuestViewTest&) = delete;

  ~KioskGuestViewTest() override = default;

 protected:
  const KioskMixin::Config& kiosk_mixin_config() { return GetParam(); }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  guest_view::TestGuestViewManagerFactory& factory() { return factory_; }

  ash::KioskMixin kiosk_{&mixin_host_,
                         /*cached_configuration=*/kiosk_mixin_config()};

 private:
  guest_view::TestGuestViewManagerFactory factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(KioskGuestViewTest, AddingWebViewGuestViewDoesNotCrash) {
  EXPECT_EQ(0ULL, GetGuestViewManager(factory()).GetCurrentGuestCount());

  // Open Web UI with a `<webview>` tag.
  // Note: `<webview>` is a restricted tag and triggers guest view creation only
  // for Chrome apps and WebUi pages, which are allowlisted here:
  // extensions/common/api/_api_features.json
  ASSERT_NE(nullptr,
            OpenUrlInBrowser(GURL("chrome://chrome-signin/?reason=5")));

  auto* guest_view =
      GetGuestViewManager(factory()).WaitForSingleGuestViewCreated();
  ASSERT_NE(guest_view, nullptr);
  ASSERT_NE(guest_view->web_contents(), nullptr);
  EXPECT_NE(nullptr, extensions::WebViewGuest::FromWebContents(
                         guest_view->web_contents()));
  EXPECT_NO_FATAL_FAILURE(NotifyKioskGuestAdded(guest_view->web_contents()));
}

IN_PROC_BROWSER_TEST_P(KioskGuestViewTest,
                       AddingMimeHandlerGuestViewDoesNotCrash) {
  // Open a new browser with locally served test-iframe-pdf.html web page.
  GURL url = kiosk_.GetDefaultServerUrl(kPdfOpenerUrl);
  content::WebContents* web_contents = OpenUrlInBrowser(url);
  ASSERT_TRUE(OpenPdfInWebContents(web_contents));

  auto* guest_view =
      GetGuestViewManager(factory()).WaitForSingleGuestViewCreated();
  ASSERT_NE(guest_view, nullptr);
  ASSERT_NE(guest_view->web_contents(), nullptr);
  EXPECT_EQ(nullptr, extensions::WebViewGuest::FromWebContents(
                         guest_view->web_contents()));
  EXPECT_NO_FATAL_FAILURE(NotifyKioskGuestAdded(guest_view->web_contents()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskGuestViewTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

class WebKioskGuestViewTest : public MixinBasedInProcessBrowserTest {
 public:
  WebKioskGuestViewTest() = default;
  WebKioskGuestViewTest(const WebKioskGuestViewTest&) = delete;
  WebKioskGuestViewTest& operator=(const WebKioskGuestViewTest&) = delete;

  ~WebKioskGuestViewTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  guest_view::TestGuestViewManagerFactory& factory() { return factory_; }

 private:
  ash::KioskMixin kiosk_{
      &mixin_host_,
      KioskMixin::Config(/*name=*/{},
                         KioskMixin::AutoLaunchAccount(PdfWebApp().account_id),
                         {PdfWebApp()})};

  guest_view::TestGuestViewManagerFactory factory_;
};

IN_PROC_BROWSER_TEST_F(WebKioskGuestViewTest,
                       AddingMimeHandlerGuestViewDoesNotCrash) {
  auto* web_contents = BrowserList::GetInstance()
                           ->get(0)
                           ->tab_strip_model()
                           ->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  if (web_contents->IsLoading()) {
    content::WaitForLoadStop(web_contents);
  }
  ASSERT_TRUE(OpenPdfInWebContents(web_contents));

  auto* guest_view =
      GetGuestViewManager(factory()).WaitForSingleGuestViewCreated();

  ASSERT_NE(guest_view, nullptr);
  ASSERT_NE(guest_view->web_contents(), nullptr);
  EXPECT_EQ(nullptr, extensions::WebViewGuest::FromWebContents(
                         guest_view->web_contents()));
  EXPECT_NO_FATAL_FAILURE(NotifyKioskGuestAdded(guest_view->web_contents()));
}

}  // namespace ash
