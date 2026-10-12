// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>

#include <array>
#include <memory>
#include <optional>
#include <string>

#include "base/format_macros.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_api.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/views_test_utils.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "extensions/browser/app_window/app_window.h"
#endif

namespace extensions {

namespace keys = tabs_constants;
namespace utils = api_test_utils;

namespace {

class TestFunctionDispatcherDelegate
    : public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  explicit TestFunctionDispatcherDelegate(BrowserWindowInterface* browser)
      : browser_(browser) {}
  ~TestFunctionDispatcherDelegate() override = default;

 private:
  extensions::WindowController* GetExtensionWindowController() override {
    return BrowserExtensionWindowController::From(browser_);
  }

  raw_ptr<BrowserWindowInterface> browser_;
};

class ExtensionWindowsTest : public ExtensionApiTest {
 public:
  ExtensionWindowsTest() = default;

  ExtensionWindowsTest(const ExtensionWindowsTest&) = delete;
  ExtensionWindowsTest& operator=(const ExtensionWindowsTest&) = delete;

  // ExtensionApiTest overrides:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  std::string GetWindowType(BrowserWindowInterface* test_browser,
                            scoped_refptr<const Extension> extension) {
    auto function = base::MakeRefCounted<WindowsGetFunction>();
    function->set_extension(extension.get());
    std::string args = base::StringPrintf(
        R"([%u, {"windowTypes": ["normal", "popup", "devtools", "app"]}])",
        ExtensionTabUtil::GetWindowId(test_browser));
    base::DictValue result =
        utils::ToDict(utils::RunFunctionAndReturnSingleResult(function.get(),
                                                              args, profile()));
    return api_test_utils::GetString(result, "type");
  }

  std::optional<base::Value> RunFunctionWithDispatcherDelegateAndReturnValue(
      scoped_refptr<ExtensionFunction> function,
      const std::string& args,
      BrowserWindowInterface* browser) {
    auto dispatcher = std::make_unique<extensions::ExtensionFunctionDispatcher>(
        browser->GetProfile());
    TestFunctionDispatcherDelegate dispatcher_delegate(browser);
    dispatcher->set_delegate(&dispatcher_delegate);
    return utils::RunFunctionWithDelegateAndReturnSingleResult(
        std::move(function), args, std::move(dispatcher),
        utils::FunctionMode::kNone);
  }
};

#if BUILDFLAG(ENABLE_PLATFORM_APPS)

using ExtensionWindowsTestWithApps = PlatformAppBrowserTest;

#endif

class ExtensionWindowCreateTest : public ExtensionBrowserTest {
 public:
  // Runs chrome.windows.create(), expecting an error.
  std::string RunCreateWindowExpectError(const std::string& args) {
    auto function = base::MakeRefCounted<WindowsCreateFunction>();
    function->set_extension(ExtensionBuilder("Test").Build().get());
    return api_test_utils::RunFunctionAndReturnError(function.get(), args,
                                                     profile());
  }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
const int kUndefinedId = INT_MIN;

int GetWindowId(const base::DictValue& window) {
  return window.FindInt(extension_misc::kId).value_or(kUndefinedId);
}
#endif

}  // namespace

#if BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest, GetWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser_window_interface());

  // Invalid window ID error.
  auto function = base::MakeRefCounted<WindowsGetFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), base::StringPrintf("[%u]", window_id + 1), profile()),
      ExtensionTabUtil::kWindowNotFoundError));

  // Basic window details.
  gfx::Rect bounds;
  if (browser_window_interface()->GetWindow()->IsMinimized()) {
    bounds = browser_window_interface()->GetWindow()->GetRestoredBounds();
  } else {
    bounds = browser_window_interface()->GetWindow()->GetBounds();
  }

  function = base::MakeRefCounted<WindowsGetFunction>();
  function->set_extension(extension.get());
  base::DictValue result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), base::StringPrintf("[%u]", window_id), profile()));
  EXPECT_EQ(window_id, GetWindowId(result));
  EXPECT_FALSE(api_test_utils::GetBoolean(result, "incognito"));
  EXPECT_EQ("normal", api_test_utils::GetString(result, "type"));
  EXPECT_EQ(bounds.x(), api_test_utils::GetInteger(result, "left"));
  EXPECT_EQ(bounds.y(), api_test_utils::GetInteger(result, "top"));
  EXPECT_EQ(bounds.width(), api_test_utils::GetInteger(result, "width"));
  EXPECT_EQ(bounds.height(), api_test_utils::GetInteger(result, "height"));

  // With "populate" enabled.
  function = base::MakeRefCounted<WindowsGetFunction>();
  function->set_extension(extension.get());
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      base::StringPrintf("[%u, {\"populate\": true}]", window_id), profile()));

  EXPECT_EQ(window_id, GetWindowId(result));
  // "populate" was enabled so tabs should be populated.
  base::ListValue tabs =
      api_test_utils::GetList(result, ExtensionTabUtil::kTabsKey);
  ASSERT_FALSE(tabs.empty());
  std::optional<int> tab0_id = tabs[0].GetDict().FindInt(extension_misc::kId);
  ASSERT_TRUE(tab0_id.has_value());
  EXPECT_GE(*tab0_id, 0);

  // TODO(aa): Can't assume window is focused. On mac, calling Activate() from a
  // browser test doesn't seem to do anything, so can't test the opposite
  // either.
  EXPECT_EQ(browser_window_interface()->GetWindow()->IsActive(),
            api_test_utils::GetBoolean(result, "focused"));

  // TODO(aa): Minimized and maximized dimensions. Is there a way to set
  // minimize/maximize programmatically?

  // Check window type.
  EXPECT_EQ("normal", GetWindowType(browser_window_interface(), extension));
  BrowserWindowInterface* test_browser = CreateBrowserWindow(
      BrowserWindowCreateParams(BrowserWindowInterface::TYPE_POPUP, profile(),
                                /*from_user_gesture=*/true));
  EXPECT_EQ("popup", GetWindowType(test_browser, extension));
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      GetTabListInterface()->GetTab(0)->GetContents(), false /* is_docked */);
  EXPECT_EQ("devtools",
            GetWindowType(DevToolsWindowTesting::Get(devtools)->browser(),
                          extension));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  test_browser = CreateBrowserWindow(BrowserWindowCreateParams::CreateForApp(
      "test-app", /*trusted_source=*/true, gfx::Rect(), profile(),
      /*user_gesture=*/true));
  EXPECT_EQ("app", GetWindowType(test_browser, extension));
  test_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForAppPopup(
          "test-app-popup", /*trusted_source=*/true, gfx::Rect(), profile(),
          /*user_gesture=*/true));
  EXPECT_EQ("popup", GetWindowType(test_browser, extension));

  // Incognito.
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  int incognito_window_id = ExtensionTabUtil::GetWindowId(incognito_browser);

  // Without "include_incognito".
  function = base::MakeRefCounted<WindowsGetFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), base::StringPrintf("[%u]", incognito_window_id),
          profile()),
      ExtensionTabUtil::kWindowNotFoundError));

  // With "include_incognito".
  function = base::MakeRefCounted<WindowsGetFunction>();
  function->set_extension(extension.get());
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      function.get(), base::StringPrintf("[%u]", incognito_window_id),
      profile(), api_test_utils::FunctionMode::kIncognito));
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest, GetCurrentWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser_window_interface());
  BrowserWindowInterface* new_browser = CreateBrowser(profile());
  int new_id = ExtensionTabUtil::GetWindowId(new_browser);

  // Get the current window using new_browser.
  auto function = base::MakeRefCounted<WindowsGetCurrentFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::DictValue result =
      utils::ToDict(RunFunctionWithDispatcherDelegateAndReturnValue(
          function.get(), "[]", new_browser));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionWithDispatcherDelegateAndReturnValue.
  EXPECT_EQ(new_id, GetWindowId(result));
  EXPECT_FALSE(result.contains(ExtensionTabUtil::kTabsKey));

  // Get the current window using the old window and make the tabs populated.
  function = base::MakeRefCounted<WindowsGetCurrentFunction>();
  function->set_extension(extension.get());
  result = utils::ToDict(RunFunctionWithDispatcherDelegateAndReturnValue(
      function.get(), "[{\"populate\": true}]", browser_window_interface()));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionWithDispatcherDelegateAndReturnValue.
  EXPECT_EQ(window_id, GetWindowId(result));
  // "populate" was enabled so tabs should be populated.
  base::ListValue tabs =
      api_test_utils::GetList(result, ExtensionTabUtil::kTabsKey);
  ASSERT_FALSE(tabs.empty());
  std::optional<int> tab0_id = tabs[0].GetDict().FindInt(extension_misc::kId);
  ASSERT_TRUE(tab0_id.has_value());
  // The tab id should not be -1 as this is a browser window.
  EXPECT_GE(*tab0_id, 0);
}

#if BUILDFLAG(ENABLE_PLATFORM_APPS)

// TODO(crbug.com/40745605): Test is flaky on Linux debug builds.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_GetAllWindows DISABLED_GetAllWindows
#else
#define MAYBE_GetAllWindows GetAllWindows
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWindowsTestWithApps, MAYBE_GetAllWindows) {
  const size_t NUM_WINDOWS = 5;
  std::set<int> window_ids;
  std::set<int> result_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser_window_interface()));

  for (size_t i = 0; i < NUM_WINDOWS - 1; ++i) {
    BrowserWindowInterface* new_browser = CreateBrowser(profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Application windows should not be accessible to extensions (app windows are
  // only accessible to the owning item).
  AppWindow* app_window = CreateTestAppWindow("{}");

  // Undocked DevTools window should not be accessible, unless included in the
  // type filter mask.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      TabListInterface::From(browser_window_interface())
          ->GetTab(0)
          ->GetContents(),
      false /* is_docked */);

  auto function = base::MakeRefCounted<WindowsGetAllFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::ListValue windows = utils::ToList(
      utils::RunFunctionAndReturnSingleResult(function.get(), "[]", profile()));

  EXPECT_EQ(window_ids.size(), windows.size());
  for (const base::Value& result_window : windows) {
    base::DictValue result_window_dict = utils::ToDict(result_window);
    result_ids.insert(GetWindowId(result_window_dict));

    // "populate" was not passed in so tabs are not populated.
    const base::ListValue* tabs =
        result_window_dict.FindList(ExtensionTabUtil::kTabsKey);
    EXPECT_FALSE(tabs);
  }
  // The returned ids should contain all the current browser instance ids.
  EXPECT_EQ(window_ids, result_ids);

  result_ids.clear();
  function = base::MakeRefCounted<WindowsGetAllFunction>();
  function->set_extension(extension.get());
  windows = utils::ToList(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"populate\": true}]", profile()));

  EXPECT_EQ(window_ids.size(), windows.size());
  for (const base::Value& result_window : windows) {
    base::DictValue result_window_dict = utils::ToDict(result_window);
    result_ids.insert(GetWindowId(result_window_dict));

    // "populate" was enabled so tabs should be populated.
    const base::ListValue* tabs =
        result_window_dict.FindList(ExtensionTabUtil::kTabsKey);
    EXPECT_TRUE(tabs);
  }
  // The returned ids should contain all the current app, browser and
  // devtools instance ids.
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);

  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTestWithApps, GetAllWindowsAllTypes) {
  const size_t NUM_WINDOWS = 5;
  std::set<int> window_ids;
  std::set<int> result_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser_window_interface()));

  for (size_t i = 0; i < NUM_WINDOWS - 1; ++i) {
    BrowserWindowInterface* new_browser = CreateBrowser(profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Application windows should not be accessible to extensions (app windows are
  // only accessible to the owning item).
  AppWindow* app_window = CreateTestAppWindow("{}");

  // Undocked DevTools window should be accessible too, since they have been
  // explicitly requested as part of the type filter mask.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      TabListInterface::From(browser_window_interface())
          ->GetTab(0)
          ->GetContents(),
      false /* is_docked */);
  window_ids.insert(ExtensionTabUtil::GetWindowId(
      DevToolsWindowTesting::Get(devtools)->browser()));

  auto function = base::MakeRefCounted<WindowsGetAllFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::ListValue windows(utils::ToList(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"windowTypes\": [\"app\", \"devtools\", \"normal\", \"panel\", "
      "\"popup\"]}]",
      profile())));

  EXPECT_EQ(window_ids.size(), windows.size());
  for (const base::Value& result_window : windows) {
    base::DictValue result_window_dict = utils::ToDict(result_window);
    result_ids.insert(GetWindowId(result_window_dict));

    // "populate" was not passed in so tabs are not populated.
    const base::ListValue* tabs =
        result_window_dict.FindList(ExtensionTabUtil::kTabsKey);
    EXPECT_FALSE(tabs);
  }
  // The returned ids should contain all the browser and devtools instance ids.
  EXPECT_EQ(window_ids, result_ids);

  result_ids.clear();
  function = base::MakeRefCounted<WindowsGetAllFunction>();
  function->set_extension(extension.get());
  windows = utils::ToList(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"populate\": true, \"windowTypes\": [\"app\", \"devtools\", "
      "\"normal\", \"panel\", \"popup\"]}]",
      profile()));

  EXPECT_EQ(window_ids.size(), windows.size());
  for (const base::Value& result_window : windows) {
    base::DictValue result_window_dict = utils::ToDict(result_window);
    result_ids.insert(GetWindowId(result_window_dict));

    // "populate" was enabled so tabs should be populated.
    const base::ListValue* tabs =
        result_window_dict.FindList(ExtensionTabUtil::kTabsKey);
    EXPECT_TRUE(tabs);
  }
  // The returned ids should contain all the browser and devtools instance ids.
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);

  CloseAppWindow(app_window);
}

#endif  // BUIDFLAG(ENABLE_PLATFORM_APPS)

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       DontCreateNormalWindowWhenIncognitoForced) {
  static const char kArgsWithExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\", \"incognito\": false }]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kForced);

  // Run with an explicit "incognito" param.
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), kArgsWithExplicitIncognitoParam, profile()),
      keys::kIncognitoModeIsForced));

  // Now try opening a normal window from incognito window.
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  // Run with an explicit "incognito" param.
  function = base::MakeRefCounted<WindowsCreateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgsWithExplicitIncognitoParam,
                                       incognito_browser->GetProfile()),
      keys::kIncognitoModeIsForced));
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       DontCreateIncognitoWindowWhenIncognitoDisabled) {
  static const char kArgs[] =
      "[{\"url\": \"about:blank\", \"incognito\": true }]";

  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);
  // Run in normal window.
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(function.get(), kArgs, profile()),
      keys::kIncognitoModeIsDisabled));

  // Run in incognito window.
  function = base::MakeRefCounted<WindowsCreateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(function.get(), kArgs,
                                       incognito_browser->GetProfile()),
      keys::kIncognitoModeIsDisabled));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest, WindowsCreate) {
  ASSERT_TRUE(RunExtensionTest("windows/create")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest, InvalidUpdateWindowState) {
  int window_id = ExtensionTabUtil::GetWindowId(browser_window_interface());

  static const char kArgsMinimizedWithFocus[] =
      "[%u, {\"state\": \"minimized\", \"focused\": true}]";
  auto function = base::MakeRefCounted<WindowsUpdateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithFocus, window_id), profile()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithoutFocus[] =
      "[%u, {\"state\": \"maximized\", \"focused\": false}]";
  function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithoutFocus, window_id), profile()),
      keys::kInvalidWindowStateError));

  static const char kArgsMinimizedWithBounds[] =
      "[%u, {\"state\": \"minimized\", \"width\": 500}]";
  function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithBounds, window_id), profile()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithBounds[] =
      "[%u, {\"state\": \"maximized\", \"width\": 500}]";
  function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithBounds, window_id), profile()),
      keys::kInvalidWindowStateError));
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest, InvalidUpdateWindowBounds) {
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());

  // Get the display bounds so we can test whether the window intersects.
  gfx::Rect displays;
  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    displays.Union(display.bounds());
  }

  int window_id = ExtensionTabUtil::GetWindowId(browser_window_interface());
  gfx::Rect window_bounds =
      browser_window_interface()->GetWindow()->GetBounds();

  static const char kArgsUpdateFunction[] = "[%u, {\"left\": %d, \"top\": %d}]";
  // We use a small value to move the window outside or inside the bounds.
  int window_offset = window_bounds.size().width() * 0.1;

  {
    // Window bounds that do not intersect with the display are not valid.
    int window_left = displays.right() + window_offset;
    int window_top = displays.bottom() + window_offset;
    auto function = base::MakeRefCounted<WindowsUpdateFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(base::MatchPattern(
        utils::RunFunctionAndReturnError(
            function.get(),
            base::StringPrintf(kArgsUpdateFunction, window_id, window_left,
                               window_top),
            profile()),
        keys::kInvalidWindowBoundsError));
  }

  {
    // Window bounds that intersect less than 50% with the display are not
    // valid.
    int window_left = displays.right() - window_offset;
    int window_top = displays.bottom() - window_offset;
    auto function = base::MakeRefCounted<WindowsUpdateFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(base::MatchPattern(
        utils::RunFunctionAndReturnError(
            function.get(),
            base::StringPrintf(kArgsUpdateFunction, window_id, window_left,
                               window_top),
            profile()),
        keys::kInvalidWindowBoundsError));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       InvalidUpdateWindowBoundsWithSmallSizes) {
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());

  // Get the display bounds so we can position the window outside of them.
  gfx::Rect displays;
  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    displays.Union(display.bounds());
  }

  int window_id = ExtensionTabUtil::GetWindowId(browser_window_interface());
  int window_left = displays.right() + 10;
  int window_top = displays.bottom() + 10;

  static const char kArgsUpdateFunction[] =
      "[%u, {\"left\": %d, \"top\": %d, \"width\": %d, \"height\": %d}]";

  // Windows with an empty or tiny size cannot be moved outside the displays.
  constexpr struct {
    int width;
    int height;
  } kSizes[] = {{0, 500}, {500, 0}, {0, 0}, {1, 1}};
  for (const auto& size : kSizes) {
    auto function = base::MakeRefCounted<WindowsUpdateFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(base::MatchPattern(
        utils::RunFunctionAndReturnError(
            function.get(),
            base::StringPrintf(kArgsUpdateFunction, window_id, window_left,
                               window_top, size.width, size.height),
            profile()),
        keys::kInvalidWindowBoundsError))
        << "size: " << size.width << "x" << size.height;
  }

  // Resizing a window to an empty size is not valid regardless of its
  // position.
  auto function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), base::StringPrintf("[%u, {\"width\": 0}]", window_id),
          profile()),
      keys::kInvalidWindowBoundsError));
}

// On Android this fails when Run() calls BaseWindow::CanResize() returns false
// due to default Android Browser Tests not having free-form windows.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       UpdatingWindowBoundsSucceedsForValidBounds) {
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());

  // Get the display bounds so we can test whether the window intersects.
  gfx::Rect displays;
  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    displays.Union(display.bounds());
  }

  int window_id = ExtensionTabUtil::GetWindowId(browser_window_interface());
  gfx::Rect window_bounds =
      browser_window_interface()->GetWindow()->GetBounds();

  static const char kArgsUpdateFunction[] = "[%u, {\"left\": %d, \"top\": %d}]";
  // We use a small value to move the window outside or inside the bounds.
  int window_offset = window_bounds.size().width() * 0.1;

  {
    // Window bounds that intersect 50% or more with the display are valid.
    int window_left = displays.right() - window_bounds.width() + window_offset;
    int window_top = displays.bottom() - window_bounds.height() + window_offset;
    auto function = base::MakeRefCounted<WindowsUpdateFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(
        utils::RunFunction(function.get(),
                           base::StringPrintf(kArgsUpdateFunction, window_id,
                                              window_left, window_top),
                           profile(), api_test_utils::FunctionMode::kNone));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       UpdatingWindowBoundsSucceedsForSmallOnScreenBounds) {
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());

  int window_id = ExtensionTabUtil::GetWindowId(browser_window_interface());

  // A tiny window is valid as long as it is fully within a display.
  const gfx::Point center =
      display::Screen::Get()->GetPrimaryDisplay().bounds().CenterPoint();
  static const char kArgsUpdateFunction[] =
      "[%u, {\"left\": %d, \"top\": %d, \"width\": 1, \"height\": 1}]";
  auto function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(
      utils::RunFunction(function.get(),
                         base::StringPrintf(kArgsUpdateFunction, window_id,
                                            center.x(), center.y()),
                         profile(), api_test_utils::FunctionMode::kNone));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       UpdateWindowStateFailsWhenNotResizable) {
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  ASSERT_FALSE(windows.empty());
  int window_id = ExtensionTabUtil::GetWindowId(windows[0]);

  auto function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());

  // Attempting to maximize on Android triggers the CanResize() check. Since
  // the standard test environment is not in desktop windowing mode (and may
  // run on older SDKs), this is expected to fail. We verify that an error is
  // returned without checking the exact string.
  std::string error = utils::RunFunctionAndReturnError(
      function.get(),
      base::StringPrintf("[%u, {\"state\": \"maximized\"}]", window_id),
      profile());
  EXPECT_FALSE(error.empty());
}

// TODO(crbug.com/491868694) Remove once overlapping tests are enabled on
// Android.
IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       UpdateWindowStateSucceedsWhenNoResizeNeeded) {
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  ASSERT_FALSE(windows.empty());
  int window_id = ExtensionTabUtil::GetWindowId(windows[0]);

  auto function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());

  // A simple update that does not require resizing bypasses the CanResize()
  // check, succeeding without an error code.
  EXPECT_TRUE(utils::RunFunction(
      function.get(),
      base::StringPrintf("[%u, {\"drawAttention\": true}]", window_id),
      profile(), api_test_utils::FunctionMode::kNone));
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       UpdateWindowStateFullscreenFailsOnAndroid) {
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  std::vector<BrowserWindowInterface*> windows =
      GetAllBrowserWindowInterfaces();
  ASSERT_FALSE(windows.empty());
  int window_id = ExtensionTabUtil::GetWindowId(windows[0]);

  auto function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());

  // Attempting to enter fullscreen on Android should explicitly fail with
  // the kUnableToEnterFullScreenAndroid error message.
  std::string error = utils::RunFunctionAndReturnError(
      function.get(),
      base::StringPrintf("[%u, {\"state\": \"fullscreen\"}]", window_id),
      profile());

  EXPECT_EQ(tabs_constants::kUnableToEnterFullScreenAndroid, error);
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest,
                       ExtensionAPICannotNavigateDevtools) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").AddAPIPermission("tabs").Build();

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      GetTabListInterface()->GetTab(0)->GetContents(), false /* is_docked */);

  auto function = base::MakeRefCounted<TabsUpdateFunction>();
  function->set_extension(extension.get());

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(
              "[%d, {\"url\":\"http://example.com\"}]",
              ExtensionTabUtil::GetTabId(
                  DevToolsWindowTesting::Get(devtools)->main_web_contents())),
          DevToolsWindowTesting::Get(devtools)->browser()->GetProfile()),
      tabs_constants::kNotAllowedForDevToolsError));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

#if BUILDFLAG(IS_MAC)
// Mac is intentionally unsupported (crbug.com/41385204).
#define MAYBE_AcceptState DISABLED_AcceptState
#else
#define MAYBE_AcceptState AcceptState
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, MAYBE_AcceptState) {
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  function->SetBrowserContextForTesting(profile());

  base::DictValue result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"state\": \"minimized\"}]", profile(),
          api_test_utils::FunctionMode::kIncognito));
  int window_id = GetWindowId(result);
  std::string error;

  WindowController* new_controller =
      ExtensionTabUtil::GetControllerFromWindowID(
          ChromeExtensionFunctionDetails(function.get()), window_id, &error);
  ASSERT_TRUE(new_controller);
  EXPECT_TRUE(error.empty());
  BrowserWindowInterface* new_browser = new_controller->GetBrowser();
  ASSERT_TRUE(new_browser);

// TODO(crbug.com/40254339): Remove this workaround if this wait is no longer
// needed.
// These builds flags are limiting the check for IsMinimized() for Linux.
// For Linux, we only check X11 window manager and not Wayland since our
// current fix only applies to X11.
#if BUILDFLAG(IS_LINUX)
  // Must be checked inside IS_LINUX to compile on windows/mac.
  // TODO(crbug.com/40252593): Find a fix/workaround for wayland and add
  // verification of IsMinimized() for as well.
  if (::ui::OzonePlatform::RunningOnX11ForTest()) {
    // DesktopWindowTreeHostX11::IsMinimized() relies on an asynchronous update
    // from the window server
    views::test::PropertyWaiter minimize_waiter(
        base::BindRepeating(
            &BrowserWindow::IsMinimized,
            base::Unretained(BrowserWindow::FromBrowser(new_browser))),
        true, TestTimeouts::action_timeout());
    EXPECT_TRUE(minimize_waiter.Wait());
  }
#else
  EXPECT_TRUE(new_controller->window()->IsMinimized());  // nocheck
#endif  // BUILDFLAG(IS_LINUX)

  function = base::MakeRefCounted<WindowsCreateFunction>();
  function->set_extension(extension.get());
  function->SetBrowserContextForTesting(profile());
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"state\": \"fullscreen\"}]", profile(),
      api_test_utils::FunctionMode::kIncognito));
  window_id = GetWindowId(result);

  new_controller = ExtensionTabUtil::GetControllerFromWindowID(
      ChromeExtensionFunctionDetails(function.get()), window_id, &error);
  ASSERT_TRUE(new_controller);
  EXPECT_TRUE(error.empty());
  ui_test_utils::WaitForBrowserSetLastActive(new_controller->GetBrowser());
  EXPECT_TRUE(new_controller->GetBrowser()->GetWindow()->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, ValidateCreateWindowState) {
  EXPECT_TRUE(
      base::MatchPattern(RunCreateWindowExpectError(
                             "[{\"state\": \"minimized\", \"focused\": true}]"),
                         keys::kInvalidWindowStateError));
  EXPECT_TRUE(base::MatchPattern(
      RunCreateWindowExpectError(
          "[{\"state\": \"maximized\", \"focused\": false}]"),
      keys::kInvalidWindowStateError));
  EXPECT_TRUE(base::MatchPattern(
      RunCreateWindowExpectError(
          "[{\"state\": \"fullscreen\", \"focused\": false}]"),
      keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      base::MatchPattern(RunCreateWindowExpectError(
                             "[{\"state\": \"minimized\", \"width\": 500}]"),
                         keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      base::MatchPattern(RunCreateWindowExpectError(
                             "[{\"state\": \"maximized\", \"width\": 500}]"),
                         keys::kInvalidWindowStateError));
  EXPECT_TRUE(
      base::MatchPattern(RunCreateWindowExpectError(
                             "[{\"state\": \"fullscreen\", \"width\": 500}]"),
                         keys::kInvalidWindowStateError));
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, ValidateCreateWindowBounds) {
  // Get the display bounds so we can test whether the window intersects.
  gfx::Rect displays;
  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    displays.Union(display.bounds());
  }

  static const char kArgsCreateFunction[] =
      "[{\"left\": %d, \"top\": %d, \"width\": %d, \"height\": %d }]";
  int window_width = 100;
  int window_height = 100;
  // We use a small value to move the window outside or inside the bounds.
  int window_offset = 10;

  {
    // Window bounds that do not intersect with the display are not valid.
    int window_left = displays.right() + window_offset;
    int window_top = displays.bottom() + window_offset;
    EXPECT_TRUE(
        base::MatchPattern(RunCreateWindowExpectError(base::StringPrintf(
                               kArgsCreateFunction, window_left, window_top,
                               window_width, window_height)),
                           keys::kInvalidWindowBoundsError));
  }

  {
    // Window bounds that intersect less than 50% with the display are not
    // valid.
    int window_left = displays.right() - window_offset;
    int window_top = displays.bottom() - window_offset;
    EXPECT_TRUE(
        base::MatchPattern(RunCreateWindowExpectError(base::StringPrintf(
                               kArgsCreateFunction, window_left, window_top,
                               window_width, window_height)),
                           keys::kInvalidWindowBoundsError));
  }

  {
    // Window bounds that intersect 50% or more with the display are valid.
    int window_left = displays.right() - window_width + window_offset;
    int window_top = displays.bottom() - window_height + window_offset;
    auto function = base::MakeRefCounted<WindowsCreateFunction>();
    function->set_extension(ExtensionBuilder("Test").Build().get());
    EXPECT_TRUE(utils::RunFunction(
        function.get(),
        base::StringPrintf(kArgsCreateFunction, window_left, window_top,
                           window_width, window_height),
        profile(), api_test_utils::FunctionMode::kNone));
  }

  {
    // Window bounds that specify size and not position should be adjusted
    // to the screen in case the window is not visible.
    // For this, update the current window bounds so the new window position
    // needs to be adjusted to fit.
    gfx::Rect current_window_bounds =
        browser_window_interface()->GetWindow()->GetBounds();
    current_window_bounds.set_x(current_window_bounds.x() +
                                current_window_bounds.width() - window_offset);
    current_window_bounds.set_y(current_window_bounds.y() +
                                current_window_bounds.height() - window_offset);
    browser_window_interface()->GetWindow()->SetBounds(current_window_bounds);

    static const char kArgsCreateFunctionOnlySize[] =
        "[{\"width\": %d, \"height\": %d }]";
    auto function = base::MakeRefCounted<WindowsCreateFunction>();
    function->set_extension(ExtensionBuilder("Test").Build().get());
    EXPECT_TRUE(
        utils::RunFunction(function.get(),
                           base::StringPrintf(kArgsCreateFunctionOnlySize,
                                              window_width, window_height),
                           profile(), api_test_utils::FunctionMode::kNone));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest,
                       ValidateCreateWindowBoundsWithSmallSizes) {
  // Get the display bounds so we can position the window outside of them.
  gfx::Rect displays;
  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    displays.Union(display.bounds());
  }

  static const char kArgsCreateFunction[] =
      "[{\"left\": %d, \"top\": %d, \"width\": %d, \"height\": %d }]";
  int window_left = displays.right() + 10;
  int window_top = displays.bottom() + 10;

  // Windows with an empty or tiny size cannot be created outside the
  // displays.
  constexpr struct {
    int width;
    int height;
  } kSizes[] = {{0, 100}, {100, 0}, {0, 0}, {1, 1}};
  for (const auto& size : kSizes) {
    EXPECT_TRUE(base::MatchPattern(
        RunCreateWindowExpectError(base::StringPrintf(kArgsCreateFunction,
                                                      window_left, window_top,
                                                      size.width, size.height)),
        keys::kInvalidWindowBoundsError))
        << "size: " << size.width << "x" << size.height;
  }

  // Bounds with an empty size are not valid regardless of position.
  EXPECT_TRUE(base::MatchPattern(
      RunCreateWindowExpectError("[{\"width\": 0, \"height\": 0 }]"),
      keys::kInvalidWindowBoundsError));

  {
    // A tiny window is valid as long as it is fully within a display.
    const gfx::Point center =
        display::Screen::Get()->GetPrimaryDisplay().bounds().CenterPoint();
    auto function = base::MakeRefCounted<WindowsCreateFunction>();
    function->set_extension(ExtensionBuilder("Test").Build().get());
    EXPECT_TRUE(utils::RunFunction(
        function.get(),
        base::StringPrintf(kArgsCreateFunction, center.x(), center.y(), 1, 1),
        profile(), api_test_utils::FunctionMode::kNone));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, CreatePopupWindowFromWebUI) {
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  function->SetBrowserContextForTesting(profile());
  function->set_source_context_type(mojom::ContextType::kUntrustedWebUi);

  const base::DictValue result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), R"([{"type": "popup"}])", profile()));
  int window_id = GetWindowId(result);

  std::string error;
  EXPECT_TRUE(ExtensionTabUtil::GetControllerFromWindowID(
      ChromeExtensionFunctionDetails(function.get()), window_id, &error));
  EXPECT_TRUE(error.empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest, OnBoundsChanged) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("windows/bounds")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  gfx::Rect rect = browser_window_interface()->GetWindow()->GetBounds();
  rect.Inset(10);
  browser_window_interface()->GetWindow()->SetBounds(rect);

  listener.Reply(base::StringPrintf(
      R"({"top": %u, "left": %u, "width": %u, "height": %u})", rect.y(),
      rect.x(), rect.width(), rect.height()));

  ASSERT_TRUE(catcher.GetNextResult());
}

// Tests the origin of tabs created through chrome.windows.create().
IN_PROC_BROWSER_TEST_F(ExtensionWindowsTest, WindowsCreate_OpenerAndOrigin) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("../simple_with_file"));
  ASSERT_TRUE(extension);

  // Navigate a tab to an extension page.
  GURL extension_url = extension->GetResourceURL("file.html");
  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, extension_url));

  const std::string extension_origin_str =
      url::Origin::Create(extension->url()).Serialize();
  constexpr char kDataURL[] = "data:text/html,<html>test</html>";
  std::string extension_url_str = extension_url.spec();
  struct TestCase {
    // The url to use in chrome.windows.create().
    std::string url;
    // If set, its value will be used to specify |setSelfAsOpener|.
    std::optional<bool> set_self_as_opener;
    // The origin we expect the new tab to be in, opaque origins will be "null".
    std::string expected_origin_str;
  };
  auto test_cases = std::to_array<TestCase>({
      // about:blank URLs.
      // With opener relationship, about:blank urls will get the extension's
      // origin, without opener relationship, they will get opaque/"null"
      // origin.
      {url::kAboutBlankURL, true, extension_origin_str},
      {url::kAboutBlankURL, false, "null"},
      {url::kAboutBlankURL, std::nullopt, "null"},

      // data:... URLs.
      // With opener relationship or not, "data:..." URLs always gets unique
      // origin, so origin will always be "null" in these cases.
      {kDataURL, true, "null"},
      {kDataURL, false, "null"},
      {kDataURL, std::nullopt, "null"},

      // chrome-extension:// URLs.
      // These always get extension origin.
      {extension_url_str, true, extension_origin_str},
      {extension_url_str, false, extension_origin_str},
      {extension_url_str, std::nullopt, extension_origin_str},
  });

  Profile* profile = this->profile();
  auto run_test_case = [&web_contents, profile](const TestCase& test_case) {
    std::string maybe_specify_set_self_as_opener;
    if (test_case.set_self_as_opener) {
      maybe_specify_set_self_as_opener =
          base::StringPrintf(", setSelfAsOpener: %s",
                             base::ToString(*test_case.set_self_as_opener));
    }
    std::string script = base::StringPrintf(
        R"( new Promise(resolve => {
              chrome.windows.create({url: '%s'%s}, (win) => {
                resolve(win.tabs[0].id);
              });
            }); )",
        test_case.url.c_str(), maybe_specify_set_self_as_opener.c_str());

    content::WebContents* new_contents = nullptr;
    {
      content::TestNavigationObserver nav_observer(GURL(test_case.url));
      nav_observer.StartWatchingNewWebContents();
      int tab_id = content::EvalJs(web_contents, script).ExtractInt();
      EXPECT_TRUE(
          ExtensionTabUtil::GetTabById(tab_id, profile, true, &new_contents));
      nav_observer.Wait();
    }
    ASSERT_TRUE(new_contents);
    ASSERT_TRUE(content::WaitForLoadStop(new_contents));

    EXPECT_EQ(test_case.expected_origin_str, EvalJs(new_contents, "origin;"));
    const bool is_opaque_origin =
        new_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque();
    EXPECT_EQ(test_case.expected_origin_str == "null", is_opaque_origin);
  };
  for (size_t i = 0; i < std::size(test_cases); ++i) {
    const auto& test_case = test_cases[i];
    SCOPED_TRACE(
        base::StringPrintf("#%" PRIuS " %s", i, test_case.url.c_str()));
    run_test_case(test_case);
  }
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace extensions
