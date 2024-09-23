// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/test/app_window_waiter.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace ash {

namespace {

void NotifyKioskGuestAdded(content::WebContents* guest_web_contents) {
  KioskController::Get().OnGuestAdded(guest_web_contents);
}

Profile& GetProfile() {
  return CHECK_DEREF(ProfileManager::GetPrimaryUserProfile());
}

void OpenWebUiWithGuestView() {
  // Enable troubleshooting tools to access a Web Ui page with `<webview>` tag.
  // Note: `<webview>` is a restricted tag and triggers guest view creation only
  // for Chrome apps and allowlisted WebUi pages.
  GetProfile().GetPrefs()->SetBoolean(prefs::kKioskTroubleshootingToolsEnabled,
                                      true);

  // Navigate to the WebUi page allowlisted here
  // extensions/common/api/_api_features.json.
  GURL signin_url{"chrome://chrome-signin/?reason=5"};
  Browser::CreateParams params =
      Browser::CreateParams(Browser::Type::TYPE_NORMAL,
                            /*profile=*/&GetProfile(),
                            /*user_gesture=*/true);
  auto& new_browser = CHECK_DEREF(Browser::Create(params));
  new_browser.window()->Show();
  ui_test_utils::NavigateToURLWithDisposition(
      &new_browser, signin_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

guest_view::TestGuestViewManager& GetGuestViewManager(
    guest_view::TestGuestViewManagerFactory& factory) {
  return CHECK_DEREF(factory.GetOrCreateTestGuestViewManager(
      &GetProfile(), extensions::ExtensionsAPIClient::Get()
                         ->CreateGuestViewManagerDelegate()));
}

}  // namespace

class WebKioskGuestViewBrowserTest : public WebKioskBaseTest {
 public:
  WebKioskGuestViewBrowserTest() = default;
  WebKioskGuestViewBrowserTest(const WebKioskGuestViewBrowserTest&) = delete;
  WebKioskGuestViewBrowserTest& operator=(const WebKioskGuestViewBrowserTest&) =
      delete;

  ~WebKioskGuestViewBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    WebKioskBaseTest::SetUpOnMainThread();
    InitializeRegularOnlineKiosk();
  }

  guest_view::TestGuestViewManagerFactory& factory() { return factory_; }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
};

IN_PROC_BROWSER_TEST_F(WebKioskGuestViewBrowserTest,
                       AddingGuestViewDoesNotCrash) {
  EXPECT_EQ(0ULL, GetGuestViewManager(factory()).GetCurrentGuestCount());
  OpenWebUiWithGuestView();

  auto* guest_view =
      GetGuestViewManager(factory()).WaitForSingleGuestViewCreated();
  ASSERT_NE(guest_view, nullptr);
  ASSERT_NE(guest_view->web_contents(), nullptr);
  EXPECT_NO_FATAL_FAILURE(NotifyKioskGuestAdded(guest_view->web_contents()));
}

class ChromeAppKioskGuestViewBrowserTest : public KioskBaseTest {
 public:
  ChromeAppKioskGuestViewBrowserTest() = default;
  ChromeAppKioskGuestViewBrowserTest(
      const ChromeAppKioskGuestViewBrowserTest&) = delete;
  ChromeAppKioskGuestViewBrowserTest& operator=(
      const ChromeAppKioskGuestViewBrowserTest&) = delete;

  ~ChromeAppKioskGuestViewBrowserTest() override = default;

 protected:
  void LaunchApp() {
    StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
    WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                                /*terminate_app=*/false,
                                /*keep_app_open=*/true);
  }

  content::WebContents* WaitForAppWindowWebContents() {
    extensions::AppWindowRegistry* app_window_registry =
        extensions::AppWindowRegistry::Get(&GetProfile());
    extensions::AppWindow& window = CHECK_DEREF(
        apps::AppWindowWaiter(app_window_registry, test_app_id()).Wait());
    return window.web_contents();
  }

  guest_view::TestGuestViewManagerFactory& factory() { return factory_; }

 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_{
      &mixin_host_,
      {{LoginManagerMixin::TestUserInfo{test_owner_account_id_}}}};

  guest_view::TestGuestViewManagerFactory factory_;
};

IN_PROC_BROWSER_TEST_F(ChromeAppKioskGuestViewBrowserTest,
                       AddingGuestViewDoesNotCrash) {
  LaunchApp();
  EXPECT_EQ(0ULL, GetGuestViewManager(factory()).GetCurrentGuestCount());
  OpenWebUiWithGuestView();
  auto* guest_view =
      GetGuestViewManager(factory()).WaitForSingleGuestViewCreated();
  ASSERT_NE(guest_view, nullptr);
  ASSERT_NE(guest_view->web_contents(), nullptr);
  EXPECT_NO_FATAL_FAILURE(NotifyKioskGuestAdded(guest_view->web_contents()));
}

}  // namespace ash
