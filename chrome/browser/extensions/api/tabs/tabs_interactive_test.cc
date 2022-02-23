// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "apps/test/app_window_waiter.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace extensions {

namespace keys = tabs_constants;
namespace utils = extension_function_test_utils;

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
  base::Value::DictStorage result =
      utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[]", new_browser));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(focused_window_id, api_test_utils::GetInteger(result, "id"));
  EXPECT_FALSE(result.contains(keys::kTabsKey));

  function = new extensions::WindowsGetLastFocusedFunction();
  function->set_extension(extension.get());
  result = utils::ToDictionary(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"populate\": true}]", browser()));

  // The id should always match the last focused window and does not depend
  // on what was passed to RunFunctionAndReturnSingleResult.
  EXPECT_EQ(focused_window_id, api_test_utils::GetInteger(result, "id"));
  // "populate" was enabled so tabs should be populated.
  api_test_utils::GetList(result, keys::kTabsKey);
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
  std::unique_ptr<base::ListValue> result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"lastFocusedWindow\":true}]", browser())));

  base::ListValue* result_tabs = result.get();
  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs->GetListDeprecated().size());
  for (const base::Value& result_tab : result_tabs->GetListDeprecated()) {
    EXPECT_EQ(focused_window_id,
              api_test_utils::GetInteger(utils::ToDictionary(result_tab),
                                         keys::kWindowIdKey));
  }

  // Get tabs NOT in the 'last focused' window called from the focused browser.
  function = new extensions::TabsQueryFunction();
  function->set_extension(extension.get());
  result = utils::ToList(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"lastFocusedWindow\":false}]", browser()));

  result_tabs = result.get();
  // We should get one tab for each extra window and one for the initial window.
  EXPECT_EQ(kExtraWindows + 1, result_tabs->GetListDeprecated().size());
  for (const base::Value& result_tab : result_tabs->GetListDeprecated()) {
    EXPECT_NE(focused_window_id,
              api_test_utils::GetInteger(utils::ToDictionary(result_tab),
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
// TODO(crbug.com/984350): Expand the test to verify that setSelfAsOpener
// param is ignored from Service Worker extension scripts.
IN_PROC_BROWSER_TEST_P(NonPersistentExtensionTabsTest, MAYBE_TabCurrentWindow) {
  ASSERT_TRUE(RunExtensionTest("tabs/current_window")) << message_;
}

// Crashes on Lacros and Linux-ozone-rel. http://crbug.com/1196709
#if BUILDFLAG(IS_CHROMEOS_LACROS) || defined(USE_OZONE)
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

  int GetTabId(const base::Value::DictStorage& dict) const;

  std::unique_ptr<base::Value> RunFunction(ExtensionFunction* function,
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
    const base::Value::DictStorage& dict) const {
  auto iter = dict.find(keys::kTabsKey);
  if (iter == dict.end() || !iter->second.is_list())
    return -2;
  const base::ListValue& tabs = base::Value::AsListValue(iter->second);
  if (tabs.GetListDeprecated().empty())
    return -2;
  const base::Value& tab = tabs.GetListDeprecated()[0];
  const base::DictionaryValue* tab_dict = nullptr;
  if (!tab.GetAsDictionary(&tab_dict))
    return -2;
  absl::optional<int> tab_id = tab_dict->FindIntKey(keys::kIdKey);
  if (!tab_id)
    return -2;
  return *tab_id;
}

std::unique_ptr<base::Value> ExtensionWindowLastFocusedTest::RunFunction(
    ExtensionFunction* function,
    const std::string& params) {
  function->set_extension(extension_.get());
  return utils::RunFunctionAndReturnSingleResult(function, params, browser());
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
    const base::Value::DictStorage result = utils::ToDictionary(
        RunFunction(function.get(), "[{\"populate\": true}]"));
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
    const base::Value::DictStorage result = utils::ToDictionary(
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
    const base::Value::DictStorage result = utils::ToDictionary(
        RunFunction(function.get(), "[{\"populate\": true}]"));
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
    const base::Value::DictStorage result = utils::ToDictionary(
        RunFunction(function.get(), "[{\"populate\": true}]"));
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
    const base::Value::DictStorage result = utils::ToDictionary(RunFunction(
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
    EXPECT_EQ(
        tabs_constants::kNoLastFocusedWindowError,
        extension_function_test_utils::RunFunctionAndReturnError(
            get_current_app_function.get(),
            "[{\"populate\": true, \"windowTypes\": [ \"app\" ]}]", browser()));
  }

  chrome::CloseWindow(normal_browser);
  chrome::CloseWindow(popup_browser);
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  CloseAppWindow(app_window);
}
#endif  // !BUILDFLAG(IS_MAC)

}  // namespace extensions
