// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_window_closer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

namespace {

void NotifyKioskGuestAdded(content::WebContents* guest_web_contents) {
  KioskController::Get().OnGuestAdded(guest_web_contents);
}

// Creates a new browser with enabled kiosk troubleshooting tools policy,
// navigates to the given `page_url` and returns it's active `WebContents`.
content::WebContents* OpenUrlInBrowser(GURL page_url) {
  // Enable troubleshooting tools to be able to open a browser in kiosk mode.
  ash::kiosk::test::CurrentProfile().GetPrefs()->SetBoolean(
      ash::prefs::kKioskTroubleshootingToolsEnabled, true);

  BrowserWindowCreateParams params(
      BrowserWindowInterface::Type::TYPE_NORMAL,
      /*profile=*/&ash::kiosk::test::CurrentProfile(),
      /*from_user_gesture=*/true);
  BrowserWindowInterface& new_browser = CHECK_DEREF(
      InProcessBrowserTest::CreateBrowserWindowForTesting(std::move(params)));
  new_browser.GetWindow()->Show();
  ui_test_utils::NavigateToURLWithDisposition(
      &new_browser, page_url,
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  return new_browser.GetTabStripModel()->GetActiveWebContents();
}

guest_view::TestGuestViewManager& GetGuestViewManager(
    guest_view::TestGuestViewManagerFactory& factory) {
  return CHECK_DEREF(factory.GetOrCreateTestGuestViewManager(
      &ash::kiosk::test::CurrentProfile(),
      extensions::ExtensionsAPIClient::Get()
          ->CreateGuestViewManagerDelegate()));
}

}  // namespace

class KioskGuestViewTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskGuestViewTest() {
    // Force allow Chrome Apps in Kiosk, since they are default disabled since
    // M138.
    scoped_feature_list_.InitFromCommandLine(
        /*enable_features=*/"AllowChromeAppsInKioskSessions",
        /*disable_features=*/"");
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

 private:
  ash::KioskMixin kiosk_{&mixin_host_,
                         /*cached_configuration=*/kiosk_mixin_config()};
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

// TODO(crbug.com/587027426): Add a test for a non-`WebViewGuest` guest view.

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskGuestViewTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

}  // namespace ash
