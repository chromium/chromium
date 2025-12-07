// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/guest_util.h"

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"

namespace glic {
namespace {

Profile& GetProfile() {
  return CHECK_DEREF(ProfileManager::GetLastUsedProfile());
}

void OpenWebUiWithGuestView(const GURL& host_url) {
  Browser::CreateParams params =
      Browser::CreateParams(Browser::Type::TYPE_NORMAL,
                            /*profile=*/&GetProfile(),
                            /*user_gesture=*/true);

  auto& new_browser = CHECK_DEREF(Browser::Create(params));
  new_browser.window()->Show();

  ui_test_utils::NavigateToURLWithDisposition(
      &new_browser, host_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

guest_view::TestGuestViewManager& GetGuestViewManager(
    guest_view::TestGuestViewManagerFactory& factory) {
  return CHECK_DEREF(factory.GetOrCreateTestGuestViewManager(
      &GetProfile(), extensions::ExtensionsAPIClient::Get()
                         ->CreateGuestViewManagerDelegate()));
}

class GuestUtilBrowserTest : public InProcessBrowserTest {
 public:
  GuestUtilBrowserTest() = default;
  GuestUtilBrowserTest(const GuestUtilBrowserTest&) = delete;
  GuestUtilBrowserTest& operator=(const GuestUtilBrowserTest&) = delete;

  ~GuestUtilBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Load blank page in glic guest view
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL, "about:blank");
  }

 protected:
  guest_view::TestGuestViewManagerFactory& factory() { return factory_; }

 private:
  glic::GlicTestEnvironment glic_test_environment_;
  guest_view::TestGuestViewManagerFactory factory_;
};

IN_PROC_BROWSER_TEST_F(GuestUtilBrowserTest, OnGuestAdded_NonGlic) {
  EXPECT_EQ(0ULL, GetGuestViewManager(factory()).GetCurrentGuestCount());

  // Load a non-glic webui containing a <webview>
  OpenWebUiWithGuestView(GURL{"chrome://chrome-signin/?reason=5"});

  auto* guest_view =
      GetGuestViewManager(factory()).WaitForSingleGuestViewCreated();
  ASSERT_NE(guest_view, nullptr);
  ASSERT_NE(guest_view->web_contents(), nullptr);

  // OnGuestAdded() should not affect this non-glic WebContents.
  EXPECT_FALSE(glic::OnGuestAdded(guest_view->web_contents()));
}

IN_PROC_BROWSER_TEST_F(GuestUtilBrowserTest, OnGuestAdded_Glic) {
  EXPECT_EQ(0ULL, GetGuestViewManager(factory()).GetCurrentGuestCount());

  OpenWebUiWithGuestView(GURL{chrome::kChromeUIGlicURL});

  auto* guest_view =
      GetGuestViewManager(factory()).WaitForSingleGuestViewCreated();
  ASSERT_NE(guest_view, nullptr);
  ASSERT_NE(guest_view->web_contents(), nullptr);

  // OnGuestAdded() should recognize the WebContents and set its base background
  // color. Can't directly test this because there is no public getter for
  // page_base_background_color_.
  EXPECT_TRUE(glic::OnGuestAdded(guest_view->web_contents()));
}

}  // namespace
}  // namespace glic
