// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_host_delegate.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

// Waits for a tab to be added. Used because Android's Navigate() function
// adds tabs asynchronously.
class TabAddedWaiter : public TabListInterfaceObserver {
 public:
  explicit TabAddedWaiter(TabListInterface* tab_list) {
    tab_list_interface_observation_.Observe(tab_list);
  }

  TabAddedWaiter(const TabAddedWaiter&) = delete;
  TabAddedWaiter& operator=(const TabAddedWaiter&) = delete;
  ~TabAddedWaiter() override = default;

  void Wait() {
    // The run loop will exit if it has already been Quit().
    run_loop_.Run();
  }

  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override {
    // If this occurs before Wait() the run loop will exit on Run().
    run_loop_.Quit();
  }

 private:
  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_interface_observation_{this};
  base::RunLoop run_loop_;
};

}  // namespace

using ChromeExtensionHostDelegateTest = PlatformBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeExtensionHostDelegateTest,
                       CreateTabWithWebContentsInForegroundTab) {
  // The browser starts with a single tab.
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(1, tab_list->GetTabCount());

  // Prepare for a tab being added.
  TabAddedWaiter waiter(tab_list);

  // Set up a web contents for the CreateTab() call.
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(GetProfile()));
  content::WebContents* expected_contents = contents.get();

  // Create a new foreground tab.
  ChromeExtensionHostDelegate delegate;
  delegate.CreateTab(std::move(contents), GURL("chrome://version"),
                     ExtensionId(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
                     blink::mojom::WindowFeatures(), /*user_gesture=*/true);

  // Wait for the tab to be added.
  waiter.Wait();

  // Tab was created.
  ASSERT_EQ(2, tab_list->GetTabCount());

  // Tab used the provided web contents.
  EXPECT_EQ(expected_contents, tab_list->GetTab(1)->GetContents());

  // Tab is active.
  EXPECT_EQ(tab_list->GetActiveTab(), tab_list->GetTab(1));
}

IN_PROC_BROWSER_TEST_F(ChromeExtensionHostDelegateTest,
                       CreateTabWithWebContentsInBackgroundTab) {
  // The browser starts with a single tab.
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(1, tab_list->GetTabCount());

  // Prepare for a tab being added.
  TabAddedWaiter waiter(tab_list);

  // Set up a web contents for the CreateTab() call.
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(GetProfile()));
  content::WebContents* expected_contents = contents.get();

  // Create a new background tab.
  ChromeExtensionHostDelegate delegate;
  delegate.CreateTab(std::move(contents), GURL("chrome://version"),
                     ExtensionId(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
                     blink::mojom::WindowFeatures(), /*user_gesture=*/true);

  // Wait for the tab to be added.
  waiter.Wait();

  // Tab was created.
  ASSERT_EQ(2, tab_list->GetTabCount());

  // Tab used the provided web contents.
  EXPECT_EQ(expected_contents, tab_list->GetTab(1)->GetContents());

  // Tab is not active.
  EXPECT_NE(tab_list->GetActiveTab(), tab_list->GetTab(1));
}

#if BUILDFLAG(IS_ANDROID)
// Tests that on Android, `TabHelpers` (e.g.
// `RequestDesktopSiteWebContentsObserverAndroid`) are attached synchronously to
// the `WebContents` inside `CreateTab()`.
IN_PROC_BROWSER_TEST_F(ChromeExtensionHostDelegateTest,
                       TabHelpersAttachedSynchronouslyOnAndroid) {
  TabListInterface* tab_list = GetTabListInterface();
  TabAddedWaiter waiter(tab_list);

  // Create a WebContents that does not have TabHelpers attached.
  std::unique_ptr<content::WebContents> contents = content::WebContents::Create(
      content::WebContents::CreateParams(GetProfile()));
  content::WebContents* raw_contents = contents.get();

  // Verify that tab helpers are not yet attached.
  ASSERT_EQ(RequestDesktopSiteWebContentsObserverAndroid::FromWebContents(
                raw_contents),
            nullptr);

  // Invoke CreateTab.
  ChromeExtensionHostDelegate delegate;
  delegate.CreateTab(std::move(contents), GURL("chrome://version"),
                     ExtensionId(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
                     blink::mojom::WindowFeatures(), /*user_gesture=*/true);

  // TabHelpers should be attached synchronously before CreateTab returns.
  EXPECT_NE(RequestDesktopSiteWebContentsObserverAndroid::FromWebContents(
                raw_contents),
            nullptr);

  // Wait for the asynchronous tab creation to finish.
  waiter.Wait();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace extensions
