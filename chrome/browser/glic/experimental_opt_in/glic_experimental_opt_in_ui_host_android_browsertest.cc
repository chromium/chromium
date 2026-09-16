// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"

#include <vector>

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host_android.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

class MockGlicExperimentalOptInUIHostDelegate
    : public GlicExperimentalOptInUIHost::Delegate {
 public:
  MOCK_METHOD(void, OnUIClosed, (bool accepted), (override));
};

// Returns the single popup-type browser window, or null if there isn't exactly
// one. Links opened from the opt-in dialog use
// WindowOpenDisposition::NEW_POPUP, which on Android materializes as a Custom
// Tab hosted in its own popup BrowserWindowInterface rather than as a tab in
// the originating window.
BrowserWindowInterface* FindSolePopupWindow() {
  BrowserWindowInterface* popup = nullptr;
  for (BrowserWindowInterface* window : GetAllBrowserWindowInterfaces()) {
    if (window->GetType() != BrowserWindowInterface::Type::TYPE_POPUP) {
      continue;
    }
    if (popup) {
      return nullptr;  // More than one popup; the caller's assumption broke.
    }
    popup = window;
  }
  return popup;
}

}  // namespace

class GlicExperimentalOptInUIHostAndroidBrowserTest : public GlicBrowserTest {
 public:
  GlicExperimentalOptInUIHostAndroidBrowserTest() = default;
  ~GlicExperimentalOptInUIHostAndroidBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       ShowAndSimulateAccept) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;

  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  // 1. Show the UI.
  host->Show(tab->GetContents());

  // 2. We expect the delegate to be notified with `accepted = true` after
  // the simulated close completes.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(true)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  // 3. Manually trigger the close (simulating user accepting the dialog).
  host->Close(/*accepted=*/true);

  // 4. Run the loop to wait for the PostTask to bounce back to the delegate.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       ShowAndSimulateReject) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;
  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  // 1. Show the UI.
  host->Show(tab->GetContents());

  // 2. We expect the delegate to be notified with `accepted = false` after
  // the simulated close completes.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  // 3. Manually trigger the close (simulating user rejecting the dialog).
  host->Close(/*accepted=*/false);

  // 4. Run the loop to wait for the PostTask to bounce back to the delegate.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       ShowAndSimulateDismissingDialog) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;
  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  // 1. Show the UI.
  host->Show(tab->GetContents());

  // 2. We expect the delegate to be notified with `accepted = false` after
  // the simulated close completes.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  // 3. Dismiss dialog.
  static_cast<GlicExperimentalOptInUIHostAndroid*>(host.get())
      ->SimulateDismissingForTesting();

  // 4. Run the loop to wait for the PostTask to bounce back to the delegate.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       ShowCalledTwice) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;
  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  // 1. Show the UI for the first time.
  host->Show(tab->GetContents());

  // 2. Call Show again while it is already active. This should safely do
  // nothing.
  host->Show(tab->GetContents());

  // 3. We expect the delegate to be notified exactly once when the UI closes.
  // Because `delegate` is a StrictMock, any extra calls will inherently fail
  // the test.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  // 4. Manually dismiss the dialog.
  static_cast<GlicExperimentalOptInUIHostAndroid*>(host.get())
      ->SimulateDismissingForTesting();

  // 5. Wait for the async callback.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       OpenLinkInNewTab_HttpUrl) {
  // Note: `embedded_test_server()` is already started by
  // GlicBrowserTest::SetUpOnMainThread(); starting it again would hit
  // DCHECK(!Started()).
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;
  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  host->Show(tab->GetContents());

  GURL target_url = embedded_test_server()->GetURL("/title1.html");

  const int tab_count_before = GetTabListInterface()->GetTabCount();
  const size_t window_count_before = GetAllBrowserWindowInterfaces().size();
  ASSERT_FALSE(FindSolePopupWindow());

  // 1. Set up an observer to catch the new WebContents (the Custom Tab)
  // being created and wait for it to navigate to our URL.
  content::TestNavigationObserver nav_observer(target_url);
  nav_observer.StartWatchingNewWebContents();

  host->OpenLinkInNewTab(target_url);

  // 2. Wait for the navigation to finish.
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(target_url, nav_observer.last_navigation_url());

  // 3. The Custom Tab lives in its own popup window, so the originating
  // window's tab list is deliberately left untouched.
  EXPECT_EQ(tab_count_before, GetTabListInterface()->GetTabCount());
  EXPECT_EQ(window_count_before + 1, GetAllBrowserWindowInterfaces().size());

  // 4. The link really was opened somewhere. This assertion is the counterpart
  // to the one in OpenLinkInNewTab_NonHttpUrlIgnored: if popups ever stop being
  // created, this fails loudly rather than letting that test pass vacuously.
  BrowserWindowInterface* popup_window = FindSolePopupWindow();
  ASSERT_TRUE(popup_window);
  TabListInterface* popup_tabs = TabListInterface::From(popup_window);
  ASSERT_TRUE(popup_tabs);
  ASSERT_EQ(1, popup_tabs->GetTabCount());
  EXPECT_EQ(target_url,
            popup_tabs->GetTab(0)->GetContents()->GetLastCommittedURL());

  // 5. Clean teardown.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  static_cast<GlicExperimentalOptInUIHostAndroid*>(host.get())
      ->SimulateDismissingForTesting();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       OpenLinkInNewTab_NonHttpUrlIgnored) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;
  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  host->Show(tab->GetContents());

  // Non-HTTP/HTTPS URLs should be ignored safely. Rejection is a synchronous
  // early return, so nothing can have been opened by the time this runs.
  const int tab_count_before = GetTabListInterface()->GetTabCount();
  const size_t window_count_before = GetAllBrowserWindowInterfaces().size();
  host->OpenLinkInNewTab(GURL("chrome://settings"));

  EXPECT_EQ(tab_count_before, GetTabListInterface()->GetTabCount());
  // No Custom Tab popup should have been spun up either. See
  // OpenLinkInNewTab_HttpUrl for the positive case this mirrors.
  EXPECT_EQ(window_count_before, GetAllBrowserWindowInterfaces().size());
  EXPECT_FALSE(FindSolePopupWindow());

  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  static_cast<GlicExperimentalOptInUIHostAndroid*>(host.get())
      ->SimulateDismissingForTesting();
  run_loop.Run();
}

}  // namespace glic
