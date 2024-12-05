// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string_view>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/load_profile.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

Browser::CreateParams CreateNewBrowserParams(Browser* initial_kiosk_browser,
                                             bool is_popup_browser) {
  return is_popup_browser
             ? Browser::CreateParams::CreateForAppPopup(
                   initial_kiosk_browser->app_name(),
                   /*trusted_source=*/true,
                   /*window_bounds=*/gfx::Rect(),
                   initial_kiosk_browser->profile(),
                   /*user_gesture=*/true)
             : Browser::CreateParams(initial_kiosk_browser->profile(),
                                     /*user_gesture=*/true);
}

Browser* OpenNewBrowser(Browser* initial_kiosk_browser, bool is_popup_browser) {
  Browser::CreateParams params =
      CreateNewBrowserParams(initial_kiosk_browser, is_popup_browser);
  Browser* new_browser = Browser::Create(params);
  new_browser->window()->Show();
  return new_browser;
}

// Returns the web app configured in Kiosk.
KioskApp TheKioskWebApp() {
  auto apps = KioskController::Get().GetApps();
  CHECK_EQ(apps.size(), 1ul);
  CHECK_EQ(apps[0].id().type, KioskAppType::kWebApp);
  return apps[0];
}

}  // namespace

// Verifies general Kiosk features that only apply to web apps.
class WebKioskTest : public MixinBasedInProcessBrowserTest {
 public:
  WebKioskTest() = default;

  WebKioskTest(const WebKioskTest&) = delete;
  WebKioskTest& operator=(const WebKioskTest&) = delete;

  ~WebKioskTest() override = default;

  NetworkStateMixin network_state_{&mixin_host_};

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/{/*name=*/{},
                                /*auto_launch_account_id=*/{},
                                {KioskMixin::SimpleWebAppOption()}}};
};

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewPopupBrowserInKioskNotAllowedByDefault) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  EXPECT_FALSE(initial_browser->profile()->GetPrefs()->GetBoolean(
      prefs::kNewWindowsInKioskAllowed));

  Browser* new_popup_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/true);

  TestBrowserClosedWaiter browser_closed_waiter{new_popup_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewRegularBrowserInKioskNotAllowedByDefault) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  EXPECT_FALSE(initial_browser->profile()->GetPrefs()->GetBoolean(
      prefs::kNewWindowsInKioskAllowed));

  Browser* new_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/false);

  TestBrowserClosedWaiter browser_closed_waiter{new_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

IN_PROC_BROWSER_TEST_F(WebKioskTest, NewPopupBrowserInKioskAllowedByPolicy) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);
  auto& session = CHECK_DEREF(KioskController::Get().GetKioskSystemSession());

  initial_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kNewWindowsInKioskAllowed, true);
  Browser* new_popup_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/true);

  EXPECT_FALSE(DidSessionCloseNewWindow(&session));
  ASSERT_NE(new_popup_browser, nullptr);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);

  EXPECT_FALSE(initial_browser->GetBrowserView().CanUserEnterFullscreen());
  EXPECT_FALSE(new_popup_browser->GetBrowserView().CanUserEnterFullscreen());
  EXPECT_TRUE(initial_browser->GetBrowserView().IsFullscreen());
  EXPECT_TRUE(new_popup_browser->GetBrowserView().IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebKioskTest,
                       NewRegularBrowserInKioskNotAllowedEvenByPolicy) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(kiosk_.LaunchManually(TheKioskWebApp()));
  ASSERT_TRUE(kiosk_.WaitSessionLaunched());

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  Browser* initial_browser = BrowserList::GetInstance()->get(0);

  initial_browser->profile()->GetPrefs()->SetBoolean(
      prefs::kNewWindowsInKioskAllowed, true);
  Browser* new_browser =
      OpenNewBrowser(initial_browser, /*is_popup_browser=*/false);

  TestBrowserClosedWaiter browser_closed_waiter{new_browser};
  ASSERT_TRUE(browser_closed_waiter.WaitUntilClosed());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
}

}  // namespace ash
