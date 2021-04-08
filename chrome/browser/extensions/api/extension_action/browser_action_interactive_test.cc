// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scrollbar_size.h"
#include "ui/views/widget/widget.h"

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
class PopupHostWatcher : public content::NotificationObserver {
 public:
  PopupHostWatcher() {
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
    // Only track lifetimes for popup window ExtensionHost instances.
    const ExtensionHost* host =
        content::Details<const ExtensionHost>(details).ptr();
    DCHECK(host);
    if (host->extension_host_type() != mojom::ViewType::kExtensionPopup)
      return;

    ++(type == NOTIFICATION_EXTENSION_HOST_CREATED ? created_ : destroyed_);
    if (!quit_closure_.is_null() && created_ == destroyed_)
      quit_closure_.Run();
  }

 private:
  content::NotificationRegistrar registrar_;
  base::RepeatingClosure quit_closure_;
  int created_ = 0;
  int destroyed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PopupHostWatcher);
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
    host_watcher_ = std::make_unique<PopupHostWatcher>();
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
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
  void EnsurePopupActive() {
    auto test_util = ExtensionActionTestHelper::Create(browser());
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
  content::WebContents* OpenPopupViaToolbar() {
    content::WindowedNotificationObserver popup_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    ExtensionActionTestHelper::Create(browser())->Press(0);
    popup_observer.Wait();
    EnsurePopupActive();
    const auto& source =
        static_cast<const content::Source<content::WebContents>&>(
            popup_observer.source());
    return source.ptr();
  }

  // Close the popup window directly.
  bool ClosePopup() {
    return ExtensionActionTestHelper::Create(browser())->HidePopup();
  }

  // Trigger a focus loss to close the popup.
  void ClosePopupViaFocusLoss() {
    EXPECT_TRUE(ExtensionActionTestHelper::Create(browser())->HasPopup());
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
        content::NotificationService::AllSources());

#if defined(OS_MAC)
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
    EXPECT_FALSE(ExtensionActionTestHelper::Create(browser())->HasPopup());

    // Wait for the notification to achieve a consistent state and verify that
    // the popup was properly torn down.
    observer.Wait();
    base::RunLoop().RunUntilIdle();
  }

  int num_popup_hosts_created() const { return host_watcher_->created(); }

 private:
  std::unique_ptr<PopupHostWatcher> host_watcher_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionInteractiveTest);
};

// Tests opening a popup using the chrome.browserAction.openPopup API. This test
// opens a popup in the starting window, closes the popup, creates a new window
// and opens a popup in the new window. Both popups should succeed in opening.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, TestOpenPopup) {
  auto browserActionBar = ExtensionActionTestHelper::Create(browser());
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
        content::OpenURLParams(GURL("about:blank"), content::Referrer(),
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
    EXPECT_TRUE(ExtensionActionTestHelper::Create(new_browser)->HasPopup());
  }
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
  ExtensionActionTestHelper::Create(new_browser)->HidePopup();
}

// Tests opening a popup in an incognito window.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, TestOpenPopupIncognito) {
  content::WindowedNotificationObserver frame_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  ASSERT_TRUE(RunExtensionTest({.name = "browser_action/open_popup",
                                .page_url = "open_popup_succeeds.html",
                                .open_in_incognito = true},
                               {.allow_in_incognito = true}))
      << message_;
  frame_observer.Wait();
  // Non-Aura Linux uses a singleton for the popup, so it looks like all windows
  // have popups if there is any popup open.
#if !((defined(OS_LINUX) || defined(OS_CHROMEOS)) && !defined(USE_AURA))
  // Starting window does not have a popup.
  EXPECT_FALSE(ExtensionActionTestHelper::Create(browser())->HasPopup());
#endif
  // Incognito window should have a popup.
  auto test_util = ExtensionActionTestHelper::Create(
      BrowserList::GetInstance()->GetLastActive());
  EXPECT_TRUE(test_util->HasPopup());
  test_util->HidePopup();
}

// Tests that an extension can open a popup in the last active incognito window
// even from a background page with a non-incognito profile.
// (crbug.com/448853)
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       TestOpenPopupIncognitoFromBackground) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("browser_action")
                        .AppendASCII("open_popup_background"),
                    {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  ExtensionTestMessageListener listener(false);
  listener.set_extension_id(extension->id());

  Browser* incognito_browser =
      OpenURLOffTheRecord(profile(), GURL("chrome://newtab/"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ(std::string("opened"), listener.message());
  auto test_util = ExtensionActionTestHelper::Create(incognito_browser);
  EXPECT_TRUE(test_util->HasPopup());
  test_util->HidePopup();
}

// Tests if there is already a popup open (by a user click or otherwise), that
// the openPopup API does not override it.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       TestOpenPopupDoesNotCloseOtherPopups) {
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
  EXPECT_TRUE(ClosePopup());
}

// Test that openPopup does not grant tab permissions like for browser action
// clicks if the activeTab permission is set.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       TestOpenPopupDoesNotGrantTabPermissions) {
  OpenPopupViaAPI(false);
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  ASSERT_FALSE(registry
                   ->GetExtensionById(last_loaded_extension_id(),
                                      ExtensionRegistry::ENABLED)
                   ->permissions_data()
                   ->HasAPIPermissionForTab(
                       sessions::SessionTabHelper::IdForTab(
                           browser()->tab_strip_model()->GetActiveWebContents())
                           .id(),
                       mojom::APIPermissionID::kTab));
  EXPECT_TRUE(ClosePopup());
}

// Test that the extension popup is closed when the browser window is focused.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, FocusLossClosesPopup1) {
  OpenPopupViaAPI(false);
  ClosePopupViaFocusLoss();
}

// Test that the extension popup is closed when the browser window is focused.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, FocusLossClosesPopup2) {
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

  EXPECT_FALSE(ExtensionActionTestHelper::Create(browser())->HasPopup());
}

IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       DeleteBrowserActionWithPopupOpen) {
  // First, we open a popup.
  OpenPopupViaAPI(false);
  auto browser_action_test_util = ExtensionActionTestHelper::Create(browser());
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

  EXPECT_TRUE(ClosePopup());
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
  // Load a first extension that can open a popup.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "browser_action/popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Open an extension popup by clicking the browser action button.
  content::WindowedNotificationObserver frame_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  ExtensionActionTestHelper::Create(browser())->InspectPopup(0);
  frame_observer.Wait();
  EXPECT_TRUE(ExtensionActionTestHelper::Create(browser())->HasPopup());

  // Close the browser window, this should not cause a crash.
  chrome::CloseWindow(browser());
}

#if defined(OS_WIN)
// Forcibly closing a browser HWND with a popup should not cause a crash.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, DestroyHWNDDoesNotCrash) {
  OpenPopupViaAPI(false);
  auto test_util = ExtensionActionTestHelper::Create(browser());
  const gfx::NativeView popup_view = test_util->GetPopupNativeView();
  EXPECT_NE(static_cast<gfx::NativeView>(nullptr), popup_view);
  const HWND popup_hwnd = views::HWNDForNativeView(popup_view);
  EXPECT_EQ(TRUE, ::IsWindow(popup_hwnd));
  const HWND browser_hwnd =
      views::HWNDForNativeView(browser()->window()->GetNativeWindow());
  EXPECT_EQ(TRUE, ::IsWindow(browser_hwnd));

  // Create a new browser window to prevent the message loop from terminating.
  browser()->OpenURL(content::OpenURLParams(
      GURL("chrome://version"), content::Referrer(),
      WindowOpenDisposition::NEW_WINDOW, ui::PAGE_TRANSITION_TYPED, false));

  // Forcibly closing the browser HWND should not cause a crash.
  EXPECT_EQ(TRUE, ::CloseWindow(browser_hwnd));
  EXPECT_EQ(TRUE, ::DestroyWindow(browser_hwnd));
  EXPECT_EQ(FALSE, ::IsWindow(browser_hwnd));
  EXPECT_EQ(FALSE, ::IsWindow(popup_hwnd));
}
#endif  // OS_WIN

class MainFrameSizeWaiter : public content::WebContentsObserver {
 public:
  MainFrameSizeWaiter(content::WebContents* web_contents,
                      const gfx::Size& size_to_wait_for)
      : content::WebContentsObserver(web_contents),
        size_to_wait_for_(size_to_wait_for) {}

  void Wait() {
    if (current_size() != size_to_wait_for_)
      run_loop_.Run();
  }

 private:
  gfx::Size current_size() {
    return web_contents()->GetContainerBounds().size();
  }

  void MainFrameWasResized(bool width_changed) override {
    if (current_size() == size_to_wait_for_)
      run_loop_.Quit();
  }

  gfx::Size size_to_wait_for_;
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, BrowserActionPopup) {
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("browser_action/popup")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // The extension's popup's size grows by |kGrowFactor| each click.
  const int kGrowFactor = 500;
  std::unique_ptr<ExtensionActionTestHelper> actions_bar =
      ExtensionActionTestHelper::Create(browser());
  gfx::Size minSize = actions_bar->GetMinPopupSize();
  gfx::Size middleSize = gfx::Size(kGrowFactor, kGrowFactor);
  gfx::Size maxSize = actions_bar->GetMaxPopupSize();

  // Ensure that two clicks will exceed the maximum allowed size.
  ASSERT_GT(minSize.height() + kGrowFactor * 2, maxSize.height());
  ASSERT_GT(minSize.width() + kGrowFactor * 2, maxSize.width());

  // Simulate a click on the browser action and verify the size of the resulting
  // popup.
  const gfx::Size kExpectedSizes[] = {minSize, middleSize, maxSize};
  for (size_t i = 0; i < base::size(kExpectedSizes); i++) {
    content::WebContentsAddedObserver popup_observer;
    actions_bar->Press(0);
    content::WebContents* popup = popup_observer.GetWebContents();

    if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
      actions_bar->WaitForExtensionsContainerLayout();
    } else {
      RunScheduledLayouts();
    }

    gfx::Size max_available_size =
        actions_bar->GetMaxAvailableSizeToFitBubbleOnScreen(0);

    // Take the screen boundaries into account for calculating the size of the
    // displayed popup
    gfx::Size expected_size = kExpectedSizes[i];
    expected_size.SetToMin(max_available_size);

    // Take the scrollbar thickness into account in the cases where one
    // dimension is adjusted leading to a scrollbar being added to the expected
    // size of the other dimension where there is space available. On Mac the
    // scrollbars are overlaid, appear on hover and don't increase the height
    // or width of the popup.
    const int kScrollbarAdjustment =
#if defined(OS_MAC)
        0;
#else
        gfx::scrollbar_size();
#endif

    expected_size.Enlarge(expected_size.height() < kExpectedSizes[i].height()
                              ? kScrollbarAdjustment
                              : 0,
                          expected_size.width() < kExpectedSizes[i].width()
                              ? kScrollbarAdjustment
                              : 0);
    expected_size.SetToMin(max_available_size);
    expected_size.SetToMin(maxSize);

    SCOPED_TRACE(testing::Message()
                 << "Test #" << i << ": size = " << expected_size.ToString());

    MainFrameSizeWaiter(popup, expected_size).Wait();
    EXPECT_EQ(expected_size, popup->GetContainerBounds().size());
    ASSERT_TRUE(actions_bar->HidePopup());
  }
}

// Test that a browser action popup can download data URLs. See
// https://crbug.com/821219
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       BrowserActionPopupDownload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browser_action/popup_download")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  content::DownloadTestObserverTerminal downloads_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  // Simulate a click on the browser action to open the popup.
  content::WebContents* popup = OpenPopupViaToolbar();
  ASSERT_TRUE(popup);
  content::ExecuteScriptAsync(popup, "run_tests()");

  // Wait for the download that this should have triggered to finish.
  downloads_observer.WaitForFinished();

  EXPECT_EQ(1u, downloads_observer.NumDownloadsSeenInState(
                    download::DownloadItem::COMPLETE));
  EXPECT_TRUE(ClosePopup());
}

// Test that we don't try and show a browser action popup with
// browserAction.openPopup if there is no toolbar (e.g., for web popup windows).
// Regression test for crbug.com/584747.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, OpenPopupOnPopup) {
  // Open a new web popup window.
  NavigateParams params(browser(), GURL("http://www.google.com/"),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_action = NavigateParams::SHOW_WINDOW;
  ui_test_utils::NavigateToURL(&params);
  Browser* popup_browser = params.browser;
  // Verify it is a popup, and it is the active window.
  ASSERT_TRUE(popup_browser);
  // The window isn't considered "active" on MacOSX for odd reasons. The more
  // important test is that it *is* considered the last active browser, since
  // that's what we check when we try to open the popup.
  // TODO(crbug.com/1115237): Now that this is an interactive test, is this
  // ifdef still necessary?
#if !defined(OS_MAC)
  EXPECT_TRUE(popup_browser->window()->IsActive());
#endif
  EXPECT_FALSE(browser()->window()->IsActive());
  EXPECT_FALSE(popup_browser->SupportsWindowFeature(Browser::FEATURE_TOOLBAR));
  EXPECT_EQ(popup_browser,
            chrome::FindLastActiveWithProfile(browser()->profile()));

  // Load up the extension, which will call chrome.browserAction.openPopup()
  // when it is loaded and verify that the popup didn't open.
  ExtensionTestMessageListener listener("ready", true);
  EXPECT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browser_action/open_popup_on_reply")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ResultCatcher catcher;
  listener.Reply(std::string());
  EXPECT_TRUE(catcher.GetNextResult()) << message_;
  EXPECT_EQ(0, num_popup_hosts_created());
}

// Watches a frame is swapped with a new frame by e.g., navigation.
class RenderFrameChangedWatcher : public content::WebContentsObserver {
 public:
  explicit RenderFrameChangedWatcher(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override {
    created_frame_ = new_host;
    run_loop_.Quit();
  }

  content::RenderFrameHost* WaitAndReturnNewFrame() {
    run_loop_.Run();
    return created_frame_;
  }

 private:
  base::RunLoop run_loop_;
  content::RenderFrameHost* created_frame_;
};

// Test that a browser action popup with a web iframe works correctly. The
// iframe is expected to run in a separate process.
// See https://crbug.com/546267.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       BrowserActionPopupWithIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browser_action/popup_with_iframe")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Simulate a click on the browser action to open the popup.
  ASSERT_TRUE(OpenPopupViaToolbar());

  // Find the RenderFrameHost associated with the iframe in the popup.
  content::RenderFrameHost* frame_host = nullptr;
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  std::set<content::RenderFrameHost*> frame_hosts =
      manager->GetRenderFrameHostsForExtension(extension->id());
  for (auto* host : frame_hosts) {
    if (host->GetFrameName() == "child_frame") {
      frame_host = host;
      break;
    }
  }

  ASSERT_TRUE(frame_host);
  EXPECT_EQ(extension->GetResourceURL("frame.html"),
            frame_host->GetLastCommittedURL());
  EXPECT_TRUE(frame_host->GetParent());

  // Navigate the popup's iframe to a (cross-site) web page, and wait for that
  // page to send a message, which will ensure that the page has loaded.
  RenderFrameChangedWatcher watcher(
      content::WebContents::FromRenderFrameHost(frame_host));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/popup_iframe.html"));
  std::string script = "location.href = '" + foo_url.spec() + "'";
  EXPECT_TRUE(ExecuteScript(frame_host, script));

  frame_host = watcher.WaitAndReturnNewFrame();

  // Confirm that the new page (popup_iframe.html) is actually loaded.
  content::DOMMessageQueue dom_message_queue(frame_host);
  std::string json;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&json));
  EXPECT_EQ("\"DONE\"", json);

  EXPECT_TRUE(ClosePopup());
}

class NavigatingExtensionPopupInteractiveTest
    : public BrowserActionInteractiveTest {
 public:
  const Extension& popup_extension() { return *popup_extension_; }
  const Extension& other_extension() { return *other_extension_; }

  void SetUpOnMainThread() override {
    BrowserActionInteractiveTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Load an extension with a pop-up.
    ASSERT_TRUE(popup_extension_ = LoadExtension(test_data_dir_.AppendASCII(
                    "browser_action/popup_with_form")));

    // Load another extension (that we can try navigating to).
    ASSERT_TRUE(other_extension_ = LoadExtension(test_data_dir_.AppendASCII(
                    "browser_action/popup_with_iframe")));
  }

  enum ExpectedNavigationStatus {
    EXPECTING_NAVIGATION_SUCCESS,
    EXPECTING_NAVIGATION_FAILURE,
  };

  void TestPopupNavigationViaGet(
      const GURL& target_url,
      ExpectedNavigationStatus expected_navigation_status) {
    std::string navigation_starting_script =
        content::JsReplace("window.location = $1;\n", target_url);
    TestPopupNavigation(target_url, expected_navigation_status,
                        navigation_starting_script);
  }

  void TestPopupNavigationViaPost(
      const GURL& target_url,
      ExpectedNavigationStatus expected_navigation_status) {
    const char kNavigationStartingScriptTemplate[] = R"(
        var form = document.getElementById('form');
        form.action = $1;
        form.submit();
    )";
    std::string navigation_starting_script =
        content::JsReplace(kNavigationStartingScriptTemplate, target_url);
    TestPopupNavigation(target_url, expected_navigation_status,
                        navigation_starting_script);
  }

 private:
  void TestPopupNavigation(const GURL& target_url,
                           ExpectedNavigationStatus expected_navigation_status,
                           std::string navigation_starting_script) {
    // Were there any failures so far (e.g. in SetUpOnMainThread)?
    ASSERT_FALSE(HasFailure());

    // Verify that the right action bar buttons are present.
    {
      std::unique_ptr<ExtensionActionTestHelper> action_bar_helper =
          ExtensionActionTestHelper::Create(browser());
      ASSERT_EQ(2, action_bar_helper->NumberOfBrowserActions());
      ASSERT_EQ(popup_extension().id(), action_bar_helper->GetExtensionId(0));
      ASSERT_EQ(other_extension().id(), action_bar_helper->GetExtensionId(1));
    }

    // Simulate a click on the browser action to open the popup.
    content::WebContents* popup = OpenPopupViaToolbar();
    ASSERT_TRUE(popup);
    GURL popup_url = popup_extension().GetResourceURL("popup.html");
    EXPECT_EQ(popup_url, popup->GetLastCommittedURL());

    // Note that the |setTimeout| call below is needed to make sure
    // ExecuteScriptAndExtractBool returns *after* a scheduled navigation has
    // already started.
    std::string script_to_execute =
        navigation_starting_script +
        "setTimeout(\n"
        "    function() { window.domAutomationController.send(true); },\n"
        "    0);\n";

    // Try to navigate the pop-up.
    bool ignored_script_result = false;
    content::WebContentsDestroyedWatcher popup_destruction_watcher(popup);
    EXPECT_TRUE(ExecuteScriptAndExtractBool(popup, script_to_execute,
                                            &ignored_script_result));
    popup = popup_destruction_watcher.web_contents();

    // Verify if the popup navigation succeeded or failed as expected.
    if (!popup) {
      // If navigation ends up in a tab, then the tab will be focused and
      // therefore the popup will be closed, destroying associated WebContents -
      // don't do any verification in this case.
      ADD_FAILURE() << "Navigation should not close extension pop-up";
    } else {
      // If the extension popup is still opened, then wait until there is no
      // load in progress, and verify whether the navigation succeeded or not.
      content::WaitForLoadStop(popup);

      // The popup should still be alive.
      ASSERT_TRUE(popup_destruction_watcher.web_contents());

      if (expected_navigation_status == EXPECTING_NAVIGATION_SUCCESS) {
        EXPECT_EQ(target_url, popup->GetLastCommittedURL())
            << "Navigation to " << target_url
            << " should succeed in an extension pop-up";
      } else {
        EXPECT_NE(target_url, popup->GetLastCommittedURL())
            << "Navigation to " << target_url
            << " should fail in an extension pop-up";
        EXPECT_THAT(
            popup->GetLastCommittedURL(),
            ::testing::AnyOf(::testing::Eq(popup_url),
                             ::testing::Eq(GURL("chrome-extension://invalid")),
                             ::testing::Eq(GURL("about:blank"))));
      }

      EXPECT_TRUE(ClosePopup());
    }

    // Make sure that the web navigation did not succeed somewhere outside of
    // the extension popup (as it might if ExtensionViewHost::OpenURLFromTab
    // forwards the navigation to Browser::OpenURL [which doesn't specify a
    // source WebContents]).
    TabStripModel* tabs = browser()->tab_strip_model();
    for (int i = 0; i < tabs->count(); i++) {
      content::WebContents* tab_contents = tabs->GetWebContentsAt(i);
      EXPECT_TRUE(WaitForLoadStop(tab_contents));
      EXPECT_NE(target_url, tab_contents->GetLastCommittedURL())
          << "Navigating an extension pop-up should not affect tabs.";
    }
  }

  const Extension* popup_extension_;
  const Extension* other_extension_;
};

// Tests that an extension pop-up cannot be navigated to a web page.
IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest, Webpage_Get) {
  GURL web_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  TestPopupNavigationViaGet(web_url, EXPECTING_NAVIGATION_FAILURE);
}
IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest, Webpage_Post) {
  GURL web_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  TestPopupNavigationViaPost(web_url, EXPECTING_NAVIGATION_FAILURE);
}

// Tests that an extension pop-up can be navigated to another page
// in the same extension.
IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest,
                       PageInSameExtension_Get) {
  GURL other_page_in_same_extension =
      popup_extension().GetResourceURL("other_page.html");
  TestPopupNavigationViaGet(other_page_in_same_extension,
                            EXPECTING_NAVIGATION_SUCCESS);
}
IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest,
                       PageInSameExtension_Post) {
  GURL other_page_in_same_extension =
      popup_extension().GetResourceURL("other_page.html");
  TestPopupNavigationViaPost(other_page_in_same_extension,
                             EXPECTING_NAVIGATION_SUCCESS);
}

// Tests that an extension pop-up cannot be navigated to a page
// in another extension.
IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest,
                       PageInOtherExtension_Get) {
  GURL other_extension_url = other_extension().GetResourceURL("other.html");
  TestPopupNavigationViaGet(other_extension_url, EXPECTING_NAVIGATION_FAILURE);
}

IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest,
                       PageInOtherExtension_Post) {
  GURL other_extension_url = other_extension().GetResourceURL("other.html");
  TestPopupNavigationViaPost(other_extension_url, EXPECTING_NAVIGATION_FAILURE);
}

// Tests that navigating an extension pop-up to a http URI that returns
// Content-Disposition: attachment; filename=...
// works: No navigation, but download shelf visible + download goes through.
IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest,
                       DownloadViaPost) {
  // Setup monitoring of the downloads.
  content::DownloadTestObserverTerminal downloads_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1,  // == wait_count (only waiting for "download-test3.gif").
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  // Navigate to a URL that replies with
  // Content-Disposition: attachment; filename=...
  // header.
  GURL download_url(
      embedded_test_server()->GetURL("foo.com", "/download-test3.gif"));
  TestPopupNavigationViaPost(download_url, EXPECTING_NAVIGATION_FAILURE);

  // Verify that "download-test3.gif got downloaded.
  downloads_observer.WaitForFinished();
  EXPECT_EQ(0u, downloads_observer.NumDangerousDownloadsSeen());
  EXPECT_EQ(1u, downloads_observer.NumDownloadsSeenInState(
                    download::DownloadItem::COMPLETE));

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath downloads_directory =
      DownloadPrefs(browser()->profile()).DownloadPath();
  EXPECT_TRUE(base::PathExists(
      downloads_directory.AppendASCII("download-test3-attachment.gif")));

  // The test verification below is applicable only to scenarios where the
  // download shelf is supported - on ChromeOS, instead of the download shelf,
  // there is a download notification in the right-bottom corner of the screen.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
#endif
}

IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest,
                       DownloadViaGet) {
  // Setup monitoring of the downloads.
  content::DownloadTestObserverTerminal downloads_observer(
      content::BrowserContext::GetDownloadManager(browser()->profile()),
      1,  // == wait_count (only waiting for "download-test3.gif").
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  // Navigate to a URL that replies with
  // Content-Disposition: attachment; filename=...
  // header.
  GURL download_url(
      embedded_test_server()->GetURL("foo.com", "/download-test3.gif"));
  TestPopupNavigationViaGet(download_url, EXPECTING_NAVIGATION_FAILURE);

  // Verify that "download-test3.gif got downloaded.
  downloads_observer.WaitForFinished();
  EXPECT_EQ(0u, downloads_observer.NumDangerousDownloadsSeen());
  EXPECT_EQ(1u, downloads_observer.NumDownloadsSeenInState(
                    download::DownloadItem::COMPLETE));

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath downloads_directory =
      DownloadPrefs(browser()->profile()).DownloadPath();
  EXPECT_TRUE(base::PathExists(
      downloads_directory.AppendASCII("download-test3-attachment.gif")));

  // The test verification below is applicable only to scenarios where the
  // download shelf is supported - on ChromeOS, instead of the download shelf,
  // there is a download notification in the right-bottom corner of the screen.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
#endif
}

}  // namespace
}  // namespace extensions
