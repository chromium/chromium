// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "apps/test/app_window_waiter.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace extensions {

namespace keys = tabs_constants;
namespace utils = api_test_utils;

using ContextType = ExtensionBrowserTest::ContextType;
using ExtensionTabsTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetLastFocusedWindow) {
  // Create a new window which making it the "last focused" window.
  // Note that "last focused" means the "top" most window.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(new_browser));

  GURL url("about:blank");
  ASSERT_TRUE(AddTabAtIndexToBrowser(new_browser, 0, url,
                                     ui::PAGE_TRANSITION_LINK, true));

  int focused_window_id =
      extensions::ExtensionTabUtil::GetWindowId(new_browser);

  scoped_refptr<extensions::WindowsGetLastFocusedFunction> function =
      new extensions::WindowsGetLastFocusedFunction();
  scoped_refptr<const extensions::Extension> extension(
      extensions::ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[]", new_browser->profile()));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(focused_window_id, api_test_utils::GetInteger(result, "id"));
  EXPECT_FALSE(result.contains(ExtensionTabUtil::kTabsKey));

  function = new extensions::WindowsGetLastFocusedFunction();
  function->set_extension(extension.get());
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"populate\": true}]", browser()->profile()));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(focused_window_id, api_test_utils::GetInteger(result, "id"));
  // "populate" was enabled so tabs should be populated.
  api_test_utils::GetList(result, ExtensionTabUtil::kTabsKey);
}

// Flaky on LaCrOS: crbug.com/1179817
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_QueryLastFocusedWindowTabs DISABLED_QueryLastFocusedWindowTabs
#else
#define MAYBE_QueryLastFocusedWindowTabs QueryLastFocusedWindowTabs
#endif
IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, MAYBE_QueryLastFocusedWindowTabs) {
  const size_t kExtraWindows = 2;
  for (size_t i = 0; i < kExtraWindows; ++i)
    CreateBrowser(browser()->profile());

  Browser* focused_window = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(focused_window));

  GURL url("about:blank");
  ASSERT_TRUE(AddTabAtIndexToBrowser(focused_window, 0, url,
                                     ui::PAGE_TRANSITION_LINK, true));
  int focused_window_id =
      extensions::ExtensionTabUtil::GetWindowId(focused_window);

  // Get tabs in the 'last focused' window called from non-focused browser.
  scoped_refptr<extensions::TabsQueryFunction> function =
      new extensions::TabsQueryFunction();
  scoped_refptr<const extensions::Extension> extension(
      extensions::ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::Value::List result_tabs(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"lastFocusedWindow\":true}]",
          browser()->profile())));

  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs.size());
  for (const base::Value& result_tab : result_tabs) {
    EXPECT_EQ(focused_window_id,
              api_test_utils::GetInteger(utils::ToDict(result_tab),
                                         keys::kWindowIdKey));
  }

  // Get tabs NOT in the 'last focused' window called from the focused browser.
  function = new extensions::TabsQueryFunction();
  function->set_extension(extension.get());
  result_tabs = utils::ToList(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"lastFocusedWindow\":false}]", browser()->profile()));

  // We should get one tab for each extra window and one for the initial window.
  EXPECT_EQ(kExtraWindows + 1, result_tabs.size());
  for (const base::Value& result_tab : result_tabs) {
    EXPECT_NE(focused_window_id,
              api_test_utils::GetInteger(utils::ToDict(result_tab),
                                         keys::kWindowIdKey));
  }
}

class NonPersistentExtensionTabsTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  NonPersistentExtensionTabsTest() : ExtensionApiTest(GetParam()) {}
  ~NonPersistentExtensionTabsTest() override = default;
  NonPersistentExtensionTabsTest(const NonPersistentExtensionTabsTest&) =
      delete;
  NonPersistentExtensionTabsTest& operator=(
      const NonPersistentExtensionTabsTest&) = delete;
};

// Crashes on Lacros only. http://crbug.com/1150133
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_TabCurrentWindow DISABLED_TabCurrentWindow
// Flakes on Linux Tests. http://crbug.com/1162432
#elif BUILDFLAG(IS_LINUX)
#define MAYBE_TabCurrentWindow DISABLED_TabCurrentWindow
#else
#define MAYBE_TabCurrentWindow TabCurrentWindow
#endif

// Tests chrome.windows.create and chrome.windows.getCurrent.
// TODO(crbug.com/40636155): Expand the test to verify that setSelfAsOpener
// param is ignored from Service Worker extension scripts.
IN_PROC_BROWSER_TEST_P(NonPersistentExtensionTabsTest, MAYBE_TabCurrentWindow) {
  ASSERT_TRUE(RunExtensionTest("tabs/current_window")) << message_;
}

// Crashes on Lacros and Linux-ozone-rel. http://crbug.com/1196709
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_OZONE)
#define MAYBE_TabGetLastFocusedWindow DISABLED_TabGetLastFocusedWindow
#else
#define MAYBE_TabGetLastFocusedWindow TabGetLastFocusedWindow
#endif

// Tests chrome.windows.getLastFocused.
IN_PROC_BROWSER_TEST_P(NonPersistentExtensionTabsTest,
                       MAYBE_TabGetLastFocusedWindow) {
  ASSERT_TRUE(RunExtensionTest("tabs/last_focused_window")) << message_;
}

// TODO(http://crbug.com/58229): The Linux and Lacros window managers
// behave differently, which complicates the test. A separate  test should
// be written for them to avoid complicating this one.
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_P(NonPersistentExtensionTabsTest, WindowSetFocus) {
  ASSERT_TRUE(RunExtensionTest("window_update/set_focus")) << message_;
}
#endif

INSTANTIATE_TEST_SUITE_P(EventPage,
                         NonPersistentExtensionTabsTest,
                         ::testing::Values(ContextType::kEventPage));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         NonPersistentExtensionTabsTest,
                         ::testing::Values(ContextType::kServiceWorker));

// TODO(llandwerlin): Activating a browser window and waiting for the
// action to happen requires views::Widget which is not available on
// MacOSX. Deactivate for now.
#if !BUILDFLAG(IS_MAC)
class ExtensionWindowLastFocusedTest : public PlatformAppBrowserTest {
 public:
  void SetUpOnMainThread() override;

  void ActivateBrowserWindow(Browser* browser);

  Browser* CreateBrowserWithEmptyTab(bool as_popup);

  int GetTabId(const base::Value::Dict& dict) const;

  std::optional<base::Value> RunFunction(ExtensionFunction* function,
                                         const std::string& params);

  const Extension* extension() { return extension_.get(); }

 private:
  // A helper class to wait for an views::Widget to become activated.
  class WidgetActivatedWaiter : public views::WidgetObserver {
   public:
    explicit WidgetActivatedWaiter(views::Widget* widget)
        : widget_(widget), waiting_(false) {
      widget_->AddObserver(this);
    }
    ~WidgetActivatedWaiter() override { widget_->RemoveObserver(this); }

    void ActivateAndWait() {
      widget_->Activate();
      if (!widget_->IsActive()) {
        waiting_ = true;
        base::RunLoop nested_run_loop(
            base::RunLoop::Type::kNestableTasksAllowed);
        quit_closure_ = nested_run_loop.QuitWhenIdleClosure();
        nested_run_loop.Run();
      }
    }

    // views::WidgetObserver:
    void OnWidgetActivationChanged(views::Widget* widget,
                                   bool active) override {
      if (widget_ == widget && waiting_) {
        waiting_ = false;
        std::move(quit_closure_).Run();
      }
    }

   private:
    raw_ptr<views::Widget> widget_;
    bool waiting_;
    base::RepeatingClosure quit_closure_;
  };

  scoped_refptr<const Extension> extension_;
};

void ExtensionWindowLastFocusedTest::SetUpOnMainThread() {
  PlatformAppBrowserTest::SetUpOnMainThread();
  extension_ = ExtensionBuilder("Test").Build();
}

void ExtensionWindowLastFocusedTest::ActivateBrowserWindow(Browser* browser) {
  BrowserView* view = BrowserView::GetBrowserViewForBrowser(browser);
  EXPECT_NE(nullptr, view);
  views::Widget* widget = view->frame();
  EXPECT_NE(nullptr, widget);
  WidgetActivatedWaiter waiter(widget);
  waiter.ActivateAndWait();
}

Browser* ExtensionWindowLastFocusedTest::CreateBrowserWithEmptyTab(
    bool as_popup) {
  Browser* new_browser;
  if (as_popup) {
    new_browser = Browser::Create(
        Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  } else {
    new_browser =
        Browser::Create(Browser::CreateParams(browser()->profile(), true));
  }
  AddBlankTabAndShow(new_browser);
  return new_browser;
}

int ExtensionWindowLastFocusedTest::GetTabId(
    const base::Value::Dict& dict) const {
  const base::Value::List* tabs = dict.FindList(ExtensionTabUtil::kTabsKey);
  if (!tabs || tabs->empty()) {
    return -2;
  }
  const base::Value::Dict* tab_dict = (*tabs)[0].GetIfDict();
  if (!tab_dict) {
    return -2;
  }
  return tab_dict->FindInt(extension_misc::kId).value_or(-2);
}

std::optional<base::Value> ExtensionWindowLastFocusedTest::RunFunction(
    ExtensionFunction* function,
    const std::string& params) {
  function->set_extension(extension_.get());
  return utils::RunFunctionAndReturnSingleResult(function, params,
                                                 browser()->profile());
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowLastFocusedTest,
                       NoDevtoolsAndAppWindows) {
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  {
    int devtools_window_id = ExtensionTabUtil::GetWindowId(
        DevToolsWindowTesting::Get(devtools)->browser());
    ActivateBrowserWindow(DevToolsWindowTesting::Get(devtools)->browser());

    scoped_refptr<WindowsGetLastFocusedFunction> function =
        new WindowsGetLastFocusedFunction();
    const base::Value::Dict result =
        utils::ToDict(RunFunction(function.get(), "[{\"populate\": true}]"));
    EXPECT_NE(devtools_window_id, api_test_utils::GetInteger(result, "id"));
  }

  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");
  {
    apps::AppWindowWaiter waiter(AppWindowRegistry::Get(browser()->profile()),
                                 app_window->extension_id());
    waiter.WaitForActivated();

    scoped_refptr<WindowsGetLastFocusedFunction> get_current_app_function =
        new WindowsGetLastFocusedFunction();
    const base::Value::Dict result = utils::ToDict(
        RunFunction(get_current_app_function.get(), "[{\"populate\": true}]"));
    int app_window_id = app_window->session_id().id();
    EXPECT_NE(app_window_id, api_test_utils::GetInteger(result, "id"));
  }

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowLastFocusedTest,
                       NoTabIdForDevToolsAndAppWindows) {
  Browser* normal_browser = CreateBrowserWithEmptyTab(false);
  {
    ActivateBrowserWindow(normal_browser);

    scoped_refptr<WindowsGetLastFocusedFunction> function =
        new WindowsGetLastFocusedFunction();
    const base::Value::Dict result =
        utils::ToDict(RunFunction(function.get(), "[{\"populate\": true}]"));
    int normal_browser_window_id =
        ExtensionTabUtil::GetWindowId(normal_browser);
    EXPECT_EQ(normal_browser_window_id,
              api_test_utils::GetInteger(result, "id"));
    EXPECT_NE(-1, GetTabId(result));
    EXPECT_EQ("normal", api_test_utils::GetString(result, "type"));
  }

  Browser* popup_browser = CreateBrowserWithEmptyTab(true);
  {
    ActivateBrowserWindow(popup_browser);

    scoped_refptr<WindowsGetLastFocusedFunction> function =
        new WindowsGetLastFocusedFunction();
    const base::Value::Dict result =
        utils::ToDict(RunFunction(function.get(), "[{\"populate\": true}]"));
    int popup_browser_window_id = ExtensionTabUtil::GetWindowId(popup_browser);
    EXPECT_EQ(popup_browser_window_id,
              api_test_utils::GetInteger(result, "id"));
    EXPECT_NE(-1, GetTabId(result));
    EXPECT_EQ("popup", api_test_utils::GetString(result, "type"));
  }

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  {
    ActivateBrowserWindow(DevToolsWindowTesting::Get(devtools)->browser());

    scoped_refptr<WindowsGetLastFocusedFunction> function =
        new WindowsGetLastFocusedFunction();
    const base::Value::Dict result = utils::ToDict(RunFunction(
        function.get(),
        "[{\"populate\": true, \"windowTypes\": [ \"devtools\" ]}]"));
    int devtools_window_id = ExtensionTabUtil::GetWindowId(
        DevToolsWindowTesting::Get(devtools)->browser());
    EXPECT_EQ(devtools_window_id, api_test_utils::GetInteger(result, "id"));
    EXPECT_EQ(-1, GetTabId(result));
    EXPECT_EQ("devtools", api_test_utils::GetString(result, "type"));
  }

  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");
  {
    apps::AppWindowWaiter waiter(AppWindowRegistry::Get(browser()->profile()),
                                 app_window->extension_id());
    waiter.WaitForActivated();

    scoped_refptr<WindowsGetLastFocusedFunction> get_current_app_function =
        new WindowsGetLastFocusedFunction();
    get_current_app_function->set_extension(extension());
    EXPECT_EQ(tabs_constants::kNoLastFocusedWindowError,
              api_test_utils::RunFunctionAndReturnError(
                  get_current_app_function.get(),
                  "[{\"populate\": true, \"windowTypes\": [ \"app\" ]}]",
                  browser()->profile()));
  }

  chrome::CloseWindow(normal_browser);
  chrome::CloseWindow(popup_browser);
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  CloseAppWindow(app_window);
}
#endif  // !BUILDFLAG(IS_MAC)

using TabsApiInteractiveTest = ExtensionApiTest;

// Tests that a window created with `focused: false` does not cover the focused
// window. Regression test for https://crbug.com/1302159.
IN_PROC_BROWSER_TEST_F(TabsApiInteractiveTest,
                       OpeningAnUnfocusedWindowDoesntCoverTheFocusedWindow) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Navigate to `url1` and ensure the browser is active.
  {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
    ui_test_utils::BrowserActivationWaiter activation_waiter(browser());
    browser()->window()->Activate();
    activation_waiter.WaitForActivation();
  }
  ASSERT_TRUE(browser()->window()->IsActive());

  // Create and load an extension that creates a new window with a tab at
  // `url2` with `focused: false` and waits for the tab to complete loading.
  static constexpr char kManifest[] =
      R"({
           "name": "Interactive Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "background.js" },
           "permissions": ["tabs"]
         })";
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           async function openUnfocusedWindow() {
             const url = '%s';
             const tabCreatedPromise = new Promise((resolve) => {
               chrome.tabs.onUpdated.addListener(
                   function listener(tabId, changeInfo, tab) {
                     if (changeInfo.status === 'complete' &&
                         tab.url === url) {
                       chrome.tabs.onUpdated.removeListener(listener);
                       resolve();
                     }
                   });
             });
             const win =
                 await chrome.windows.create({focused: false, url: url});
             chrome.test.assertFalse(win.focused);
             await tabCreatedPromise;
             chrome.test.succeed();
           },
         ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(kBackgroundJs, url2.spec().c_str()));

  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  // Now, verify the browsers. There should be exactly two browser windows (the
  // original and the one created by the extension).
  BrowserList* browser_list = BrowserList::GetInstance();
  ASSERT_EQ(2u, browser_list->size());
  ASSERT_TRUE(base::Contains(*browser_list, browser()));
  // Find the new browser. Be flexible in case BrowserList's internal sort
  // changes.
  Browser* new_browser = browser_list->get(0) == browser()
                             ? browser_list->get(1)
                             : browser_list->get(0);
  EXPECT_NE(new_browser, browser());

  // The new browser should have a tab pointed to `url2`; we use this mostly as
  // validation that setup went according to plan.
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
  EXPECT_EQ(url2, new_browser->tab_strip_model()
                      ->GetActiveWebContents()
                      ->GetLastCommittedURL());

  bool check_window_active_state = true;
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    BUILDFLAG(IS_OZONE_WAYLAND)
  check_window_active_state = false;
#endif

  // The new browser should be inactive, since it was created with
  // `focused: false`. The old browser should remain active.
  // This assertion fails on Wayland. This is possibly due to
  // https://crbug.com/1280332, where bubbles are drawn on the same window,
  // but that is yet to be confirmed.
  if (check_window_active_state) {
    EXPECT_FALSE(new_browser->window()->IsActive());
    EXPECT_TRUE(browser()->window()->IsActive());
  }

  // The old browser (which retains focus) should be on top of the new browser.
  // This currently fails because WidgetTest::IsWindowStackedAbove() doesn't
  // work for different BrowserViews. While the functionality is currently
  // correct, this means we don't have a good regression test for it.
  // TODO(crbug.com/40058935): Fix this.
  // EXPECT_TRUE(views::test::WidgetTest::IsWindowStackedAbove(
  //     BrowserView::GetBrowserViewForBrowser(browser())->frame(),
  //     BrowserView::GetBrowserViewForBrowser(new_browser)->frame()));
}

}  // namespace extensions
