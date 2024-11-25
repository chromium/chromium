// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/check_deref.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
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

class KioskGuestViewTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskGuestViewTest() = default;
  KioskGuestViewTest(const KioskGuestViewTest&) = delete;
  KioskGuestViewTest& operator=(const KioskGuestViewTest&) = delete;

  ~KioskGuestViewTest() override = default;

 protected:
  const KioskMixin::Config& kiosk_mixin_config() { return GetParam(); }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(kiosk_.WaitSessionLaunched());
  }

  guest_view::TestGuestViewManagerFactory& factory() { return factory_; }

 private:
  ash::KioskMixin kiosk_{&mixin_host_,
                         /*cached_configuration=*/kiosk_mixin_config()};

  guest_view::TestGuestViewManagerFactory factory_;
};

IN_PROC_BROWSER_TEST_P(KioskGuestViewTest, AddingGuestViewDoesNotCrash) {
  EXPECT_EQ(0ULL, GetGuestViewManager(factory()).GetCurrentGuestCount());
  OpenWebUiWithGuestView();

  auto* guest_view =
      GetGuestViewManager(factory()).WaitForSingleGuestViewCreated();
  ASSERT_NE(guest_view, nullptr);
  ASSERT_NE(guest_view->web_contents(), nullptr);
  EXPECT_NO_FATAL_FAILURE(NotifyKioskGuestAdded(guest_view->web_contents()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskGuestViewTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

}  // namespace ash
