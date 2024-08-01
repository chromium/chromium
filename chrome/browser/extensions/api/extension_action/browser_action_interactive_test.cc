// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
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

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/ui/download/download_display.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace extensions {
namespace {

#if !BUILDFLAG(IS_CHROMEOS)
bool IsDownloadSurfaceVisible(BrowserWindow* window) {
  return window->GetDownloadBubbleUIController()
      ->GetDownloadDisplayController()
      ->download_display_for_testing()
      ->IsShowingDetails();
}
#endif

// Helper to ensure all extension hosts are destroyed during the test. If a host
// is still alive, the Profile can not be destroyed in
// BrowserProcessImpl::StartTearDown().
// TODO(tapted): The existence of this helper is probably a bug. Extension
// hosts do not currently block shutdown the way a browser tab does. Maybe they
// should. See http://crbug.com/729476.
class PopupHostWatcher : public ExtensionHostRegistry::Observer {
 public:
  explicit PopupHostWatcher(content::BrowserContext* browser_context) {
    host_registry_observation_.Observe(
        ExtensionHostRegistry::Get(browser_context));
  }

  PopupHostWatcher(const PopupHostWatcher&) = delete;
  PopupHostWatcher& operator=(const PopupHostWatcher&) = delete;

  void Wait() {
    if (created_ == destroyed_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, quit_closure_, TestTimeouts::action_timeout());
    run_loop.Run();
  }

  int created() const { return created_; }
  int destroyed() const { return destroyed_; }

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostRenderProcessReady(
      content::BrowserContext* browser_context,
      ExtensionHost* host) override {
    // Only track lifetimes for popup window ExtensionHost instances.
    if (host->extension_host_type() != mojom::ViewType::kExtensionPopup)
      return;

    ++created_;
    QuitIfSatisfied();
  }

  void OnExtensionHostDestroyed(content::BrowserContext* browser_context,
                                ExtensionHost* host) override {
    // Only track lifetimes for popup window ExtensionHost instances.
    if (host->extension_host_type() != mojom::ViewType::kExtensionPopup)
      return;

    ++destroyed_;
    QuitIfSatisfied();
  }

 private:
  void QuitIfSatisfied() {
    if (!quit_closure_.is_null() && created_ == destroyed_)
      quit_closure_.Run();
  }

  base::RepeatingClosure quit_closure_;
  int created_ = 0;
  int destroyed_ = 0;
  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      host_registry_observation_{this};
};

// chrome.browserAction API tests that interact with the UI in such a way that
// they cannot be run concurrently (i.e. openPopup API tests that require the
// window be focused/active).
class BrowserActionInteractiveTest : public ExtensionApiTest {
 public:
  BrowserActionInteractiveTest() {}

  BrowserActionInteractiveTest(const BrowserActionInteractiveTest&) = delete;
  BrowserActionInteractiveTest& operator=(const BrowserActionInteractiveTest&) =
      delete;

  ~BrowserActionInteractiveTest() override {}

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_watcher_ = std::make_unique<PopupHostWatcher>(profile());
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  }

  void TearDownOnMainThread() override {
    // Note browser windows are closed in PostRunTestOnMainThread(), which is
    // called after this. But relying on the window close to close the
    // extension host can cause flakes. See http://crbug.com/729476.
    // Waiting here requires individual tests to ensure their popup has closed.
    host_watcher_->Wait();
    EXPECT_EQ(host_watcher_->created(), host_watcher_->destroyed());
    // Destroy the PopupHostWatcher to ensure it stops watching the profile.
    host_watcher_.reset();
    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  void EnsurePopupActive() {
    auto test_util = ExtensionActionTestHelper::Create(browser());
    EXPECT_TRUE(test_util->HasPopup());
    ASSERT_NO_FATAL_FAILURE(test_util->WaitForPopup());
    EXPECT_TRUE(test_util->HasPopup());
  }

  // Open an extension popup via the chrome.browserAction.openPopup API.
  // If |will_reply| is true, then the listener is responsible for having a
  // test message listener that replies to the extension. Otherwise, this
  // method will create a listener and reply to the extension before returning
  // to avoid leaking an API function while waiting for a reply.
  void OpenPopupViaAPI(bool will_reply) {
    // Setup the observer to wait for the popup to finish loading.
    content::CreateAndLoadWebContentsObserver frame_observer;
    std::unique_ptr<ExtensionTestMessageListener> listener;
    if (!will_reply)
      listener = std::make_unique<ExtensionTestMessageListener>("ready");
    // Show first popup in first window and expect it to have loaded.
    ASSERT_TRUE(RunExtensionTest("browser_action/open_popup",
                                 {.extension_url = "open_popup_succeeds.html"}))
        << message_;
    if (listener)
      EXPECT_TRUE(listener->WaitUntilSatisfied());
    frame_observer.Wait();
    EnsurePopupActive();
  }

  // Open an extension popup by clicking the browser action button associated
  // with `id`.
  content::WebContents* OpenPopupViaToolbar(const std::string& id) {
    EXPECT_FALSE(id.empty());
    content::CreateAndLoadWebContentsObserver popup_observer;
    ExtensionActionTestHelper::Create(browser())->Press(id);
    content::WebContents* popup = popup_observer.Wait();
    EnsurePopupActive();
    return popup;
  }

  // Close the popup window directly.
  bool ClosePopup() {
    return ExtensionActionTestHelper::Create(browser())->HidePopup();
  }

  // Trigger a focus loss to close the popup.
  void ClosePopupViaFocusLoss() {
    EXPECT_TRUE(ExtensionActionTestHelper::Create(browser())->HasPopup());

    ExtensionHostTestHelper host_helper(profile());

#if BUILDFLAG(IS_MAC)
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
    host_helper.WaitForHostDestroyed();
    base::RunLoop().RunUntilIdle();
  }

  int num_popup_hosts_created() const { return host_watcher_->created(); }

 private:
  std::unique_ptr<PopupHostWatcher> host_watcher_;
};

// Tests opening a popup using the chrome.browserAction.openPopup API. This test
// opens a popup in the starting window, closes the popup, creates a new window
// and opens a popup in the new window. Both popups should succeed in opening.
// TODO(crbug.com/40781224): Test flaking frequently on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TestOpenPopup DISABLED_TestOpenPopup
#else
#define MAYBE_TestOpenPopup TestOpenPopup
#endif
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, MAYBE_TestOpenPopup) {
  auto browserActionBar = ExtensionActionTestHelper::Create(browser());
  // Setup extension message listener to wait for javascript to finish running.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  {
    OpenPopupViaAPI(true);
    EXPECT_TRUE(browserActionBar->HasPopup());
    browserActionBar->HidePopup();
  }

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  Browser* new_browser = nullptr;
  {
    content::CreateAndLoadWebContentsObserver frame_observer;
    // Open a new window.
    new_browser = chrome::FindBrowserWithTab(browser()->OpenURL(
        content::OpenURLParams(GURL("about:blank"), content::Referrer(),
                               WindowOpenDisposition::NEW_WINDOW,
                               ui::PAGE_TRANSITION_TYPED, false),
        /*navigation_handle_callback=*/{}));
    // Pin the extension to test that it opens when the action is on the
    // toolbar.
    ToolbarActionsModel::Get(profile())->SetActionVisibility(
        last_loaded_extension_id(), true);
    frame_observer.Wait();
  }

  EXPECT_TRUE(new_browser != nullptr);

  ResultCatcher catcher;
  {
    content::CreateAndLoadWebContentsObserver frame_observer;
    // Show second popup in new window.
    listener.Reply("show another");
    frame_observer.Wait();
    EXPECT_TRUE(ExtensionActionTestHelper::Create(new_browser)->HasPopup());
  }
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
  ExtensionActionTestHelper::Create(new_browser)->HidePopup();
}

// Tests opening a popup in an incognito window.
// TODO(crbug.com/345091943): Extremely flaky on Mac release builds.
#if BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_TestOpenPopupIncognito DISABLED_TestOpenPopupIncognito
#else
#define MAYBE_TestOpenPopupIncognito TestOpenPopupIncognito
#endif
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       MAYBE_TestOpenPopupIncognito) {
  // The creation of the incognito window is the first WebContents.
  content::CreateAndLoadWebContentsObserver frame_observer(
      /*num_expected_contents=*/2);
  ASSERT_TRUE(RunExtensionTest(
      "browser_action/open_popup",
      {.extension_url = "open_popup_succeeds.html", .open_in_incognito = true},
      {.allow_in_incognito = true}))
      << message_;
  frame_observer.Wait();
  // Non-Aura Linux uses a singleton for the popup, so it looks like all windows
  // have popups if there is any popup open.
#if !((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !defined(USE_AURA))
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
  ExtensionTestMessageListener listener;
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
  const Extension* first_extension =
      LoadExtension(test_data_dir_.AppendASCII("browser_action/popup"));
  ASSERT_TRUE(first_extension) << message_;

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  // Load the test extension which will do nothing except notifyPass() to
  // return control here.
  ASSERT_TRUE(RunExtensionTest("browser_action/open_popup",
                               {.extension_url = "open_popup_fails.html"}))
      << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Open a popup with the first extension.
  OpenPopupViaToolbar(first_extension->id());
  ResultCatcher catcher;
  // Now, try to open a popup with the second extension. It should fail since
  // there's an active popup.
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
  ASSERT_FALSE(registry->enabled_extensions()
                   .GetByID(last_loaded_extension_id())
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
  OpenPopupViaToolbar(extension->id());
  ClosePopupViaFocusLoss();
}

// TODO(crbug.com/330684964): Test flaking frequently on Linux MSan builder
// and Mac release builders.
#if (BUILDFLAG(IS_MAC) && defined(NDEBUG)) || \
    (BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER))
#define MAYBE_TabSwitchClosesPopup DISABLED_TabSwitchClosesPopup
#else
#define MAYBE_TabSwitchClosesPopup TabSwitchClosesPopup
#endif
// Test that the extension popup is closed on browser tab switches.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest,
                       MAYBE_TabSwitchClosesPopup) {
  // Add a second tab to the browser and open an extension popup.
  chrome::NewTab(browser());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1),
            browser()->tab_strip_model()->GetActiveWebContents());
  OpenPopupViaAPI(false);

  ExtensionHostTestHelper host_helper(profile());
  // Change active tabs, the extension popup should close.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  host_helper.WaitForHostDestroyed();

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("popup.html")));
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
  ExtensionHostTestHelper host_helper(profile(), extension->id());
  OpenPopupViaToolbar(extension->id());
  ExtensionHost* extension_host = host_helper.WaitForRenderProcessReady();
  ASSERT_TRUE(extension_host);
  content::WebContents* popup_contents = extension_host->host_contents();

  // The popup should not use the per-origin zoom level that was set by zooming
  // the tab.
  const double default_zoom_level =
      content::HostZoomMap::GetForWebContents(popup_contents)
          ->GetDefaultZoomLevel();
  double popup_zoom_level = content::HostZoomMap::GetZoomLevel(popup_contents);
  EXPECT_TRUE(blink::ZoomValuesEqual(popup_zoom_level, default_zoom_level))
      << popup_zoom_level << " vs " << default_zoom_level;

  // Preventing the use of the per-origin zoom level in the popup should not
  // affect the zoom of the tab.
  EXPECT_TRUE(blink::ZoomValuesEqual(zoom_controller->GetZoomLevel(),
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
  EXPECT_TRUE(blink::ZoomValuesEqual(popup_zoom_level, default_zoom_level))
      << popup_zoom_level << " vs " << default_zoom_level;

  EXPECT_TRUE(ClosePopup());
}

class BrowserActionInteractiveViewsTest : public BrowserActionInteractiveTest {
 public:
  BrowserActionInteractiveViewsTest() = default;

  BrowserActionInteractiveViewsTest(const BrowserActionInteractiveViewsTest&) =
      delete;
  BrowserActionInteractiveViewsTest& operator=(
      const BrowserActionInteractiveViewsTest&) = delete;

  ~BrowserActionInteractiveViewsTest() override = default;
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
  // This creates two WebContents: the popup and the DevTools window.
  content::CreateAndLoadWebContentsObserver frame_observer(
      /*num_expected_contents=*/2);
  ExtensionActionTestHelper::Create(browser())->InspectPopup(extension->id());
  frame_observer.Wait();
  EXPECT_TRUE(ExtensionActionTestHelper::Create(browser())->HasPopup());

  // Close the browser window, this should not cause a crash.
  chrome::CloseWindow(browser());
}

#if BUILDFLAG(IS_WIN)
// Forcibly closing a browser HWND with a popup should not cause a crash.
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, DestroyHWNDDoesNotCrash) {
  OpenPopupViaAPI(false);
  auto test_util = ExtensionActionTestHelper::Create(browser());
  const gfx::NativeView popup_view = test_util->GetPopupNativeView();
  EXPECT_NE(gfx::NativeView(), popup_view);
  const HWND popup_hwnd = views::HWNDForNativeView(popup_view);
  EXPECT_EQ(TRUE, ::IsWindow(popup_hwnd));
  const HWND browser_hwnd =
      views::HWNDForNativeView(browser()->window()->GetNativeWindow());
  EXPECT_EQ(TRUE, ::IsWindow(browser_hwnd));

  // Create a new browser window to prevent the message loop from terminating.
  browser()->OpenURL(
      content::OpenURLParams(GURL("chrome://version"), content::Referrer(),
                             WindowOpenDisposition::NEW_WINDOW,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

  // Forcibly closing the browser HWND should not cause a crash.
  EXPECT_EQ(TRUE, ::CloseWindow(browser_hwnd));
  EXPECT_EQ(TRUE, ::DestroyWindow(browser_hwnd));
  EXPECT_EQ(FALSE, ::IsWindow(browser_hwnd));
  EXPECT_EQ(FALSE, ::IsWindow(popup_hwnd));
}
#endif  // BUILDFLAG(IS_WIN)

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

  void PrimaryMainFrameWasResized(bool width_changed) override {
    if (current_size() == size_to_wait_for_)
      run_loop_.Quit();
  }

  gfx::Size size_to_wait_for_;
  base::RunLoop run_loop_;
};

// TODO(crbug.com/40791502): Test crashes on Windows
#if BUILDFLAG(IS_WIN)
#define MAYBE_BrowserActionPopup DISABLED_BrowserActionPopup
#elif BUILDFLAG(IS_LINUX) && \
    (defined(THREAD_SANITIZER) || defined(ADDRESS_SANITIZER))
// TODO(crbug.com/40803969): Test is flaky for linux tsan and asan builds
#define MAYBE_BrowserActionPopup DISABLED_BrowserActionPopup
#elif BUILDFLAG(IS_MAC)
// TODO(crbug.com/40803969): Test is flaky on Mac as well.
#define MAYBE_BrowserActionPopup DISABLED_BrowserActionPopup
#else
#define MAYBE_BrowserActionPopup BrowserActionPopup
#endif
IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveTest, MAYBE_BrowserActionPopup) {
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
  // popup. It is important to do minSize last, because all browser actions
  // start at minSize and later increase in size. The MainFrameSizeWaiter won't
  // wait for the popup.js code to run in the minSize case, which can prevent it
  // from setting and storing the size for the next iteration, resulting in test
  // flakiness.
  const gfx::Size kExpectedSizes[] = {maxSize, middleSize, minSize};
  for (size_t i = 0; i < std::size(kExpectedSizes); i++) {
    content::WebContentsAddedObserver popup_observer;
    actions_bar->Press(extension->id());
    content::WebContents* popup = popup_observer.GetWebContents();
    actions_bar->WaitForExtensionsContainerLayout();

    gfx::Size max_available_size =
        actions_bar->GetMaxAvailableSizeToFitBubbleOnScreen(extension->id());

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
#if BUILDFLAG(IS_MAC)
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
      browser()->profile()->GetDownloadManager(), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  // Simulate a click on the browser action to open the popup.
  content::WebContents* popup = OpenPopupViaToolbar(extension->id());
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
  // TODO(crbug.com/40711219): Now that this is an interactive test, is this
  // ifdef still necessary?
#if !BUILDFLAG(IS_MAC)
  ui_test_utils::BrowserActivationWaiter waiter(popup_browser);
  waiter.WaitForActivation();
  EXPECT_TRUE(popup_browser->window()->IsActive());
#endif
  EXPECT_FALSE(browser()->window()->IsActive());
  EXPECT_FALSE(popup_browser->SupportsWindowFeature(Browser::FEATURE_TOOLBAR));
  EXPECT_EQ(popup_browser,
            chrome::FindLastActiveWithProfile(browser()->profile()));

  // Load up the extension, which will call chrome.browserAction.openPopup()
  // when it is loaded and verify that the popup didn't open.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
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
  raw_ptr<content::RenderFrameHost> created_frame_;
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
  ASSERT_TRUE(OpenPopupViaToolbar(extension->id()));

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
  EXPECT_TRUE(ExecJs(frame_host, script));

  frame_host = watcher.WaitAndReturnNewFrame();

  // Confirm that the new page (popup_iframe.html) is actually loaded.
  content::DOMMessageQueue dom_message_queue(frame_host);
  std::string json;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&json));
  EXPECT_EQ("\"DONE\"", json);

  EXPECT_TRUE(ClosePopup());
}

class BrowserActionInteractiveFencedFrameTest
    : public BrowserActionInteractiveTest {
 public:
  ~BrowserActionInteractiveFencedFrameTest() override = default;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

IN_PROC_BROWSER_TEST_F(BrowserActionInteractiveFencedFrameTest,
                       BrowserActionPopupWithFencedFrame) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browser_action/popup_with_fencedframe")));
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  // Simulate a click on the browser action to open the popup.
  ASSERT_TRUE(OpenPopupViaToolbar(extension->id()));

  // Find a primary main frame associated in the popup.
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  std::set<content::RenderFrameHost*> hosts =
      manager->GetRenderFrameHostsForExtension(extension->id());
  const auto& it = base::ranges::find_if(
      hosts, &content::RenderFrameHost::IsInPrimaryMainFrame);
  content::RenderFrameHost* primary_render_frame_host =
      (it != hosts.end()) ? *it : nullptr;
  ASSERT_TRUE(primary_render_frame_host);

  // Navigate the popup's fenced frame to a (cross-site) web page via its
  // parent, and wait for that page to send a message, which will ensure that
  // the page has loaded.
  GURL foo_url(https_server.GetURL("a.test", "/popup_fencedframe.html"));

  content::TestNavigationObserver observer(
      content::WebContents::FromRenderFrameHost(primary_render_frame_host));
  std::string script =
      "document.querySelector('fencedframe').config = new FencedFrameConfig('" +
      foo_url.spec() + "')";
  EXPECT_TRUE(ExecJs(primary_render_frame_host, script));
  observer.WaitForNavigationFinished();

  content::RenderFrameHost* fenced_frame_render_frame_host =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          primary_render_frame_host);
  ASSERT_TRUE(fenced_frame_render_frame_host);

  // Confirm that the new page (popup_fencedframe.html) is actually loaded.
  content::DOMMessageQueue dom_message_queue(fenced_frame_render_frame_host);
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
      EXPECT_TRUE(action_bar_helper->HasAction(popup_extension().id()));
      EXPECT_TRUE(action_bar_helper->HasAction(other_extension().id()));
    }

    // Simulate a click on the browser action to open the popup.
    content::WebContents* popup = OpenPopupViaToolbar(popup_extension().id());
    ASSERT_TRUE(popup);
    GURL popup_url = popup_extension().GetResourceURL("popup.html");
    EXPECT_EQ(popup_url, popup->GetLastCommittedURL());

    // Note that the |setTimeout| call below is needed to make sure EvalJs
    // returns *after* a scheduled navigation has already started.
    std::string script_to_execute = navigation_starting_script +
                                    "new Promise(resolve => {\n"
                                    "  setTimeout(\n"
                                    "    function() { resolve(true); },\n"
                                    "    0);\n"
                                    "});\n";

    // Try to navigate the pop-up.
    content::WebContentsDestroyedWatcher popup_destruction_watcher(popup);
    EXPECT_TRUE(ExecJs(popup, script_to_execute));
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

  raw_ptr<const Extension, DanglingUntriaged> popup_extension_;
  raw_ptr<const Extension, DanglingUntriaged> other_extension_;
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
      browser()->profile()->GetDownloadManager(),
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
  // download surface is supported - on ChromeOS, instead of the download
  // surface, there is a download notification in the right-bottom corner of the
  // screen.
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsDownloadSurfaceVisible(browser()->window()));
#endif
}

IN_PROC_BROWSER_TEST_F(NavigatingExtensionPopupInteractiveTest,
                       DownloadViaGet) {
  // Setup monitoring of the downloads.
  content::DownloadTestObserverTerminal downloads_observer(
      browser()->profile()->GetDownloadManager(),
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
  // download surface is supported - on ChromeOS, instead of the download
  // surface, there is a download notification in the right-bottom corner of the
  // screen.
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsDownloadSurfaceVisible(browser()->window()));
#endif
}

}  // namespace
}  // namespace extensions
