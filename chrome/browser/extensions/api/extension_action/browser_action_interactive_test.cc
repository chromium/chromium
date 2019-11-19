// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/buildflags.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace extensions {
namespace {

// Helper to ensure all extension hosts are destroyed during the test. If a host
// is still alive, the Profile can not be destroyed in
// BrowserProcessImpl::StartTearDown(). TODO(tapted): The existence of this
// helper is probably a bug. Extension hosts do not currently block shutdown the
// way a browser tab does. Maybe they should. See http://crbug.com/729476.
class ExtensionHostWatcher : public content::NotificationObserver {
 public:
  ExtensionHostWatcher() {
    registrar_.Add(this, NOTIFICATION_EXTENSION_HOST_CREATED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, NOTIFICATION_EXTENSION_HOST_DESTROYED,
                   content::NotificationService::AllSources());
  }

  void Wait() {
    if (created_ == destroyed_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, quit_closure_, TestTimeouts::action_timeout());
    run_loop.Run();
  }

  int created() const { return created_; }
  int destroyed() const { return destroyed_; }

  // NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ++(type == NOTIFICATION_EXTENSION_HOST_CREATED ? created_ : destroyed_);
    if (!quit_closure_.is_null() && created_ == destroyed_)
      quit_closure_.Run();
  }

 private:
  content::NotificationRegistrar registrar_;
  base::Closure quit_closure_;
  int created_ = 0;
  int destroyed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHostWatcher);
};

// chrome.browserAction API tests that interact with the UI in such a way that
// they cannot be run concurrently (i.e. openPopup API tests that require the
// window be focused/active).
class BrowserActionInteractiveTest : public ExtensionApiTest {
 public:
  BrowserActionInteractiveTest() {}
  ~BrowserActionInteractiveTest() override {}

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    host_watcher_ = std::make_unique<ExtensionHostWatcher>();
    ExtensionApiTest::SetUpOnMainThread();
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

  void TearDownOnMainThread() override {
    // Note browser windows are closed in PostRunTestOnMainThread(), which is
    // called after this. But relying on the window close to close the
    // extension host can cause flakes. See http://crbug.com/729476.
    // Waiting here requires individual tests to ensure their popup has closed.
    ExtensionApiTest::TearDownOnMainThread();
    host_watcher_->Wait();
    EXPECT_EQ(host_watcher_->created(), host_watcher_->destroyed());
  }

 protected:
  // Function to control whether to run popup tests for the current platform.
  // These tests require RunExtensionSubtest to work as expected and the browser
  // window to able to be made active automatically. Returns false for platforms
  // where these conditions are not met.
  bool ShouldRunPopupTest() {
    // TODO(justinlin): http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
    return false;
#else
    return true;
#endif
  }

  void EnsurePopupActive() {
    auto test_util = BrowserActionTestUtil::Create(browser());
    EXPECT_TRUE(test_util->HasPopup());
    EXPECT_TRUE(test_util->WaitForPopup());
    EXPECT_TRUE(test_util->HasPopup());
  }

  // Open an extension popup via the chrome.browserAction.openPopup API.
  // If |will_reply| is true, then the listener is responsible for having a
  // test message listener that replies to the extension. Otherwise, this
  // method will create a listener and reply to the extension before returning
  // to avoid leaking an API function while waiting for a reply.
  void OpenPopupViaAPI(bool will_reply) {
    // Setup the notification observer to wait for the popup to finish loading.
    content::WindowedNotificationObserver frame_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    std::unique_ptr<ExtensionTestMessageListener> listener;
    if (!will_reply)
      listener = std::make_unique<ExtensionTestMessageListener>("ready", false);
    // Show first popup in first window and expect it to have loaded.
    ASSERT_TRUE(RunExtensionSubtest("browser_action/open_popup",
                                    "open_popup_succeeds.html")) << message_;
    if (listener)
      EXPECT_TRUE(listener->WaitUntilSatisfied());
    frame_observer.Wait();
    EnsurePopupActive();
  }

  // Open an extension popup by clicking the browser action button.
  void OpenPopupViaToolbar() {
    content::WindowedNotificationObserver frame_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    BrowserActionTestUtil::Create(browser())->Press(0);
    frame_observer.Wait();
    EnsurePopupActive();
  }

  // Close the popup window directly.
  void ClosePopup() { BrowserActionTestUtil::Create(browser())->HidePopup(); }

  // Trigger a focus loss to close the popup.
  void ClosePopupViaFocusLoss() {
    EXPECT_TRUE(BrowserActionTestUtil::Create(browser())->HasPopup());
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());

#if defined(OS_MACOSX)
    // ClickOnView() in an inactive window is not robust on Mac. The click does
    // not guarantee window activation on trybots. So activate the browser
    // explicitly, thus causing the bubble to lose focus and dismiss itself.
    // This works because bubbles on Mac are always toplevel.
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
#else
    // Elsewhere, click on the omnibox. Note that with aura, the browser may be
    // "active" the entire time when the popup is not a toplevel window. It's
    // aura::Window::Focus() that determines where key events go in this case.
    ui_test_utils::ClickOnView(browser(), VIEW_ID_OMNIBOX);
#endif

    // The window disappears immediately.
    EXPECT_FALSE(BrowserActionTestUtil::Create(browser())->HasPopup());

    // Wait for the notification to achieve a consistent state and verify that
    // the popup was properly torn down.
    observer.Wait();
    base::RunLoop().RunUntilIdle();
  }

 private:
  std::unique_ptr<ExtensionHostWatcher> host_watcher_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionInteractiveTest);
};

// Tests opening a popup using the chrome.browserAction.openPopup API. This test
// opens a popup in the starting window, closes the popup, creates a new window
// and opens a popup in the new window. Both popups should succeed in opening.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, TestOpenPopup) {
  if (!ShouldRunPopupTest())
    return;

  auto browserActionBar = BrowserActionTestUtil::Create(browser());
  // Setup extension message listener to wait for javascript to finish running.
  ExtensionTestMessageListener listener("ready", true);
  {
    OpenPopupViaAPI(true);
    EXPECT_TRUE(browserActionBar->HasPopup());
    browserActionBar->HidePopup();
  }

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  Browser* new_browser = NULL;
  {
    content::WindowedNotificationObserver frame_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    // Open a new window.
    new_browser = chrome::FindBrowserWithWebContents(browser()->OpenURL(
        content::OpenURLParams(GURL("about:"), content::Referrer(),
                               WindowOpenDisposition::NEW_WINDOW,
                               ui::PAGE_TRANSITION_TYPED, false)));
    // Hide all the buttons to test that it opens even when the browser action
    // is in the overflow bucket.
    ToolbarActionsModel::Get(profile())->SetVisibleIconCount(0);
    frame_observer.Wait();
  }

  EXPECT_TRUE(new_browser != NULL);

  ResultCatcher catcher;
  {
    content::WindowedNotificationObserver frame_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    // Show second popup in new window.
    listener.Reply("show another");
    frame_observer.Wait();
    EXPECT_TRUE(BrowserActionTestUtil::Create(new_browser)->HasPopup());
  }
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
  BrowserActionTestUtil::Create(new_browser)->HidePopup();
}

// Tests opening a popup in an incognito window.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, TestOpenPopupIncognito) {
  if (!ShouldRunPopupTest())
    return;

  content::WindowedNotificationObserver frame_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  ASSERT_TRUE(RunExtensionSubtest("browser_action/open_popup",
                                  "open_popup_succeeds.html",
                                  kFlagEnableIncognito | kFlagUseIncognito))
      << message_;
  frame_observer.Wait();
  // Non-Aura Linux uses a singleton for the popup, so it looks like all windows
  // have popups if there is any popup open.
#if !(defined(OS_LINUX) && !defined(USE_AURA))
  // Starting window does not have a popup.
  EXPECT_FALSE(BrowserActionTestUtil::Create(browser())->HasPopup());
#endif
  // Incognito window should have a popup.
  auto test_util = BrowserActionTestUtil::Create(
      BrowserList::GetInstance()->GetLastActive());
  EXPECT_TRUE(test_util->HasPopup());
  test_util->HidePopup();
}

// Tests that an extension can open a popup in the last active incognito window
// even from a background page with a non-incognito profile.
// (crbug.com/448853)
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       TestOpenPopupIncognitoFromBackground) {
  if (!ShouldRunPopupTest())
    return;

  const Extension* extension =
      LoadExtensionIncognito(test_data_dir_.AppendASCII("browser_action").
          AppendASCII("open_popup_background"));
  ASSERT_TRUE(extension);
  ExtensionTestMessageListener listener(false);
  listener.set_extension_id(extension->id());

  Browser* incognito_browser =
      OpenURLOffTheRecord(profile(), GURL("chrome://newtab/"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ(std::string("opened"), listener.message());
  auto test_util = BrowserActionTestUtil::Create(incognito_browser);
  EXPECT_TRUE(test_util->HasPopup());
  test_util->HidePopup();
}

// Tests if there is already a popup open (by a user click or otherwise), that
// the openPopup API does not override it.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       TestOpenPopupDoesNotCloseOtherPopups) {
  if (!ShouldRunPopupTest())
    return;

  // Load a first extension that can open a popup.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  ExtensionTestMessageListener listener("ready", true);
  // Load the test extension which will do nothing except notifyPass() to
  // return control here.
  ASSERT_TRUE(RunExtensionSubtest("browser_action/open_popup",
                                  "open_popup_fails.html")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  OpenPopupViaToolbar();
  ResultCatcher catcher;
  // Return control to javascript to validate that opening a popup fails now.
  listener.Reply("show another");
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
  ClosePopup();
}

// Test that openPopup does not grant tab permissions like for browser action
// clicks if the activeTab permission is set.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       TestOpenPopupDoesNotGrantTabPermissions) {
  if (!ShouldRunPopupTest())
    return;

  OpenPopupViaAPI(false);
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  ASSERT_FALSE(registry
                   ->GetExtensionById(last_loaded_extension_id(),
                                      ExtensionRegistry::ENABLED)
                   ->permissions_data()
                   ->HasAPIPermissionForTab(
                       SessionTabHelper::IdForTab(
                           browser()->tab_strip_model()->GetActiveWebContents())
                           .id(),
                       APIPermission::kTab));
  ClosePopup();
}

// Test that the extension popup is closed when the browser window is focused.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, FocusLossClosesPopup1) {
  if (!ShouldRunPopupTest())
    return;
  OpenPopupViaAPI(false);
  ClosePopupViaFocusLoss();
}

// Test that the extension popup is closed when the browser window is focused.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, FocusLossClosesPopup2) {
  if (!ShouldRunPopupTest())
    return;

  // Load a first extension that can open a popup.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;
  OpenPopupViaToolbar();
  ClosePopupViaFocusLoss();
}

// Test that the extension popup is closed on browser tab switches.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, TabSwitchClosesPopup) {
  if (!ShouldRunPopupTest())
    return;

  // Add a second tab to the browser and open an extension popup.
  chrome::NewTab(browser());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1),
            browser()->tab_strip_model()->GetActiveWebContents());
  OpenPopupViaAPI(false);

  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());
  // Change active tabs, the extension popup should close.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  observer.Wait();

  EXPECT_FALSE(BrowserActionTestUtil::Create(browser())->HasPopup());
}

IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       DeleteBrowserActionWithPopupOpen) {
  if (!ShouldRunPopupTest())
    return;

  // First, we open a popup.
  OpenPopupViaAPI(false);
  auto browser_action_test_util = BrowserActionTestUtil::Create(browser());
  EXPECT_TRUE(browser_action_test_util->HasPopup());

  // Then, find the extension that created it.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);
  GURL url = active_web_contents->GetLastCommittedURL();
  const Extension* extension = ExtensionRegistry::Get(browser()->profile())->
      enabled_extensions().GetExtensionOrAppByURL(url);
  ASSERT_TRUE(extension);

  // Finally, uninstall the extension, which causes the view to be deleted and
  // the popup to go away. This should not crash.
  UninstallExtension(extension->id());
  EXPECT_FALSE(browser_action_test_util->HasPopup());
}

IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, PopupZoomsIndependently) {
  if (!ShouldRunPopupTest())
    return;

  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("browser_action/open_popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Navigate to one of the extension's pages in a tab.
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL("popup.html"));
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Zoom the extension page in the tab.
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(tab_contents);
  double tab_old_zoom_level = zoom_controller->GetZoomLevel();
  double tab_new_zoom_level = tab_old_zoom_level + 1.0;
  zoom::ZoomController::ZoomChangedEventData zoom_change_data(
      tab_contents, tab_old_zoom_level, tab_new_zoom_level,
      zoom::ZoomController::ZOOM_MODE_DEFAULT, true);
  zoom::ZoomChangedWatcher zoom_change_watcher(tab_contents, zoom_change_data);
  zoom_controller->SetZoomLevel(tab_new_zoom_level);
  zoom_change_watcher.Wait();

  // Open the extension's popup.
  content::WindowedNotificationObserver popup_observer(
      NOTIFICATION_EXTENSION_HOST_CREATED,
      content::NotificationService::AllSources());
  OpenPopupViaToolbar();
  popup_observer.Wait();
  ExtensionHost* extension_host =
      content::Details<ExtensionHost>(popup_observer.details()).ptr();
  content::WebContents* popup_contents = extension_host->host_contents();

  // The popup should not use the per-origin zoom level that was set by zooming
  // the tab.
  const double default_zoom_level =
      content::HostZoomMap::GetForWebContents(popup_contents)
          ->GetDefaultZoomLevel();
  double popup_zoom_level = content::HostZoomMap::GetZoomLevel(popup_contents);
  EXPECT_TRUE(blink::PageZoomValuesEqual(popup_zoom_level, default_zoom_level))
      << popup_zoom_level << " vs " << default_zoom_level;

  // Preventing the use of the per-origin zoom level in the popup should not
  // affect the zoom of the tab.
  EXPECT_TRUE(blink::PageZoomValuesEqual(zoom_controller->GetZoomLevel(),
                                         tab_new_zoom_level))
      << zoom_controller->GetZoomLevel() << " vs " << tab_new_zoom_level;

  // Subsequent zooming in the tab should also be done independently of the
  // popup.
  tab_old_zoom_level = zoom_controller->GetZoomLevel();
  tab_new_zoom_level = tab_old_zoom_level + 1.0;
  zoom::ZoomController::ZoomChangedEventData zoom_change_data2(
      tab_contents, tab_old_zoom_level, tab_new_zoom_level,
      zoom::ZoomController::ZOOM_MODE_DEFAULT, true);
  zoom::ZoomChangedWatcher zoom_change_watcher2(tab_contents,
                                                zoom_change_data2);
  zoom_controller->SetZoomLevel(tab_new_zoom_level);
  zoom_change_watcher2.Wait();

  popup_zoom_level = content::HostZoomMap::GetZoomLevel(popup_contents);
  EXPECT_TRUE(blink::PageZoomValuesEqual(popup_zoom_level, default_zoom_level))
      << popup_zoom_level << " vs " << default_zoom_level;

  ClosePopup();
}

class BrowserActionInteractiveViewsTest : public BrowserActionInteractiveTest {
 public:
  BrowserActionInteractiveViewsTest() = default;
  ~BrowserActionInteractiveViewsTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserActionInteractiveViewsTest);
};

// Test closing the browser while inspecting an extension popup with dev tools.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveViewsTest,
                       CloseBrowserWithDevTools) {
  if (!ShouldRunPopupTest())
    return;

  // Load a first extension that can open a popup.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Open an extension popup by clicking the browser action button.
  content::WindowedNotificationObserver frame_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  BrowserActionTestUtil::Create(browser())->InspectPopup(0);
  frame_observer.Wait();
  EXPECT_TRUE(BrowserActionTestUtil::Create(browser())->HasPopup());

  // Close the browser window, this should not cause a crash.
  chrome::CloseWindow(browser());
}

#if defined(OS_WIN)
// Forcibly closing a browser HWND with a popup should not cause a crash.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, DestroyHWNDDoesNotCrash) {
  if (!ShouldRunPopupTest())
    return;

  OpenPopupViaAPI(false);
  auto test_util = BrowserActionTestUtil::Create(browser());
  const gfx::NativeView popup_view = test_util->GetPopupNativeView();
  EXPECT_NE(static_cast<gfx::NativeView>(nullptr), popup_view);
  const HWND popup_hwnd = views::HWNDForNativeView(popup_view);
  EXPECT_EQ(TRUE, ::IsWindow(popup_hwnd));
  const HWND browser_hwnd =
      views::HWNDForNativeView(browser()->window()->GetNativeWindow());
  EXPECT_EQ(TRUE, ::IsWindow(browser_hwnd));

  // Create a new browser window to prevent the message loop from terminating.
  browser()->OpenURL(content::OpenURLParams(GURL("about:"), content::Referrer(),
                                            WindowOpenDisposition::NEW_WINDOW,
                                            ui::PAGE_TRANSITION_TYPED, false));

  // Forcibly closing the browser HWND should not cause a crash.
  EXPECT_EQ(TRUE, ::CloseWindow(browser_hwnd));
  EXPECT_EQ(TRUE, ::DestroyWindow(browser_hwnd));
  EXPECT_EQ(FALSE, ::IsWindow(browser_hwnd));
  EXPECT_EQ(FALSE, ::IsWindow(popup_hwnd));
}
#endif  // OS_WIN

}  // namespace
}  // namespace extensions
