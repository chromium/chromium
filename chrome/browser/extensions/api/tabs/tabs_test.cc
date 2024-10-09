// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/test/scoped_fake_nswindow_fullscreen.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "base/test/with_feature_override.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
#include "pdf/pdf_features.h"
#endif

namespace extensions {

namespace keys = tabs_constants;
namespace utils = api_test_utils;

namespace {

// Returns true if |val| contains any privacy information, e.g. url,
// pendingUrl, title or faviconUrl.
bool HasAnyPrivacySensitiveFields(const base::Value::Dict& dict) {
  constexpr std::array privacySensitiveKeys{
      tabs_constants::kUrlKey, tabs_constants::kTitleKey,
      tabs_constants::kFaviconUrlKey, tabs_constants::kPendingUrlKey};
  for (auto* key : privacySensitiveKeys) {
    if (dict.contains(key)) {
      return true;
    }
  }
  return false;
}

class TestFunctionDispatcherDelegate
    : public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  explicit TestFunctionDispatcherDelegate(Browser* browser)
      : browser_(browser) {}
  ~TestFunctionDispatcherDelegate() override = default;

 private:
  extensions::WindowController* GetExtensionWindowController() const override {
    return browser_->extension_window_controller();
  }

  content::WebContents* GetAssociatedWebContents() const override {
    return nullptr;
  }

  raw_ptr<Browser> browser_;
};

class ExtensionTabsTest : public PlatformAppBrowserTest {
 public:
  ExtensionTabsTest() {}

  ExtensionTabsTest(const ExtensionTabsTest&) = delete;
  ExtensionTabsTest& operator=(const ExtensionTabsTest&) = delete;

  std::string GetWindowType(Browser* test_browser,
                            scoped_refptr<const Extension> extension) {
    auto function = base::MakeRefCounted<WindowsGetFunction>();
    function->set_extension(extension.get());
    std::string args = base::StringPrintf(
        R"([%u, {"windowTypes": ["normal", "popup", "devtools", "app"]}])",
        ExtensionTabUtil::GetWindowId(test_browser));
    base::Value::Dict result =
        utils::ToDict(utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()->profile()));
    return api_test_utils::GetString(result, "type");
  }

  std::optional<base::Value> RunFunctionWithDispatcherDelegateAndReturnValue(
      scoped_refptr<ExtensionFunction> function,
      const std::string& args,
      Browser* browser) {
    auto dispatcher = std::make_unique<extensions::ExtensionFunctionDispatcher>(
        browser->profile());
    TestFunctionDispatcherDelegate dispatcher_delegate(browser);
    dispatcher->set_delegate(&dispatcher_delegate);
    return utils::RunFunctionWithDelegateAndReturnSingleResult(
        std::move(function), args, std::move(dispatcher),
        utils::FunctionMode::kNone);
  }
};

class ExtensionWindowCreateTest : public InProcessBrowserTest {
 public:
  // Runs chrome.windows.create(), expecting an error.
  std::string RunCreateWindowExpectError(const std::string& args) {
    auto function = base::MakeRefCounted<WindowsCreateFunction>();
    function->set_extension(ExtensionBuilder("Test").Build().get());
    return api_test_utils::RunFunctionAndReturnError(function.get(), args,
                                                     browser()->profile());
  }
};

const int kUndefinedId = INT_MIN;
const ExtensionTabUtil::ScrubTabBehavior kDontScrubBehavior = {
    ExtensionTabUtil::kDontScrubTab, ExtensionTabUtil::kDontScrubTab};

int GetTabId(const base::Value::Dict& tab) {
  return tab.FindInt(extension_misc::kId).value_or(kUndefinedId);
}

int GetTabWindowId(const base::Value::Dict& tab) {
  return tab.FindInt(keys::kWindowIdKey).value_or(kUndefinedId);
}

int GetWindowId(const base::Value::Dict& window) {
  return window.FindInt(extension_misc::kId).value_or(kUndefinedId);
  ;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Invalid window ID error.
  auto function = base::MakeRefCounted<WindowsGetFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), base::StringPrintf("[%u]", window_id + 1),
          browser()->profile()),
      ExtensionTabUtil::kWindowNotFoundError));

  // Basic window details.
  gfx::Rect bounds;
  if (browser()->window()->IsMinimized())
    bounds = browser()->window()->GetRestoredBounds();
  else
    bounds = browser()->window()->GetBounds();

  function = base::MakeRefCounted<WindowsGetFunction>();
  function->set_extension(extension.get());
  base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), base::StringPrintf("[%u]", window_id),
          browser()->profile()));
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
      base::StringPrintf("[%u, {\"populate\": true}]", window_id),
      browser()->profile()));

  EXPECT_EQ(window_id, GetWindowId(result));
  // "populate" was enabled so tabs should be populated.
  base::Value::List tabs =
      api_test_utils::GetList(result, ExtensionTabUtil::kTabsKey);
  ASSERT_FALSE(tabs.empty());
  std::optional<int> tab0_id = tabs[0].GetDict().FindInt(extension_misc::kId);
  ASSERT_TRUE(tab0_id.has_value());
  EXPECT_GE(*tab0_id, 0);

  // TODO(aa): Can't assume window is focused. On mac, calling Activate() from a
  // browser test doesn't seem to do anything, so can't test the opposite
  // either.
  EXPECT_EQ(browser()->window()->IsActive(),
            api_test_utils::GetBoolean(result, "focused"));

  // TODO(aa): Minimized and maximized dimensions. Is there a way to set
  // minimize/maximize programmatically?

  // Check window type.
  EXPECT_EQ("normal", GetWindowType(browser(), extension));
  Browser* test_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  EXPECT_EQ("popup", GetWindowType(test_browser, extension));
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  EXPECT_EQ("devtools",
            GetWindowType(DevToolsWindowTesting::Get(devtools)->browser(),
                          extension));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
  test_browser = Browser::Create(Browser::CreateParams::CreateForApp(
      "test-app", true, gfx::Rect(), browser()->profile(), true));
  EXPECT_EQ("app", GetWindowType(test_browser, extension));
  test_browser = Browser::Create(Browser::CreateParams::CreateForAppPopup(
      "test-app-popup", true, gfx::Rect(), browser()->profile(), true));
  EXPECT_EQ("popup", GetWindowType(test_browser, extension));

  // Incognito.
  Browser* incognito_browser = CreateIncognitoBrowser();
  int incognito_window_id = ExtensionTabUtil::GetWindowId(incognito_browser);

  // Without "include_incognito".
  function = base::MakeRefCounted<WindowsGetFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(), base::StringPrintf("[%u]", incognito_window_id),
          browser()->profile()),
      ExtensionTabUtil::kWindowNotFoundError));

  // With "include_incognito".
  function = base::MakeRefCounted<WindowsGetFunction>();
  function->set_extension(extension.get());
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      function.get(), base::StringPrintf("[%u]", incognito_window_id),
      browser()->profile(), api_test_utils::FunctionMode::kIncognito));
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetCurrentWindow) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());
  Browser* new_browser = CreateBrowser(browser()->profile());
  int new_id = ExtensionTabUtil::GetWindowId(new_browser);

  // Get the current window using new_browser.
  auto function = base::MakeRefCounted<WindowsGetCurrentFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::Value::Dict result =
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
      function.get(), "[{\"populate\": true}]", browser()));

  // The id should match the window id of the browser instance that was passed
  // to RunFunctionWithDispatcherDelegateAndReturnValue.
  EXPECT_EQ(window_id, GetWindowId(result));
  // "populate" was enabled so tabs should be populated.
  base::Value::List tabs =
      api_test_utils::GetList(result, ExtensionTabUtil::kTabsKey);
  ASSERT_FALSE(tabs.empty());
  std::optional<int> tab0_id = tabs[0].GetDict().FindInt(extension_misc::kId);
  ASSERT_TRUE(tab0_id.has_value());
  // The tab id should not be -1 as this is a browser window.
  EXPECT_GE(*tab0_id, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetAllWindows) {
  const size_t NUM_WINDOWS = 5;
  std::set<int> window_ids;
  std::set<int> result_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));

  for (size_t i = 0; i < NUM_WINDOWS - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Application windows should not be accessible to extensions (app windows are
  // only accessible to the owning item).
  AppWindow* app_window = CreateTestAppWindow("{}");

  // Undocked DevTools window should not be accessible, unless included in the
  // type filter mask.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  auto function = base::MakeRefCounted<WindowsGetAllFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::Value::List windows =
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[]", browser()->profile()));

  EXPECT_EQ(window_ids.size(), windows.size());
  for (const base::Value& result_window : windows) {
    base::Value::Dict result_window_dict = utils::ToDict(result_window);
    result_ids.insert(GetWindowId(result_window_dict));

    // "populate" was not passed in so tabs are not populated.
    const base::Value::List* tabs =
        result_window_dict.FindList(ExtensionTabUtil::kTabsKey);
    EXPECT_FALSE(tabs);
  }
  // The returned ids should contain all the current browser instance ids.
  EXPECT_EQ(window_ids, result_ids);

  result_ids.clear();
  function = base::MakeRefCounted<WindowsGetAllFunction>();
  function->set_extension(extension.get());
  windows = utils::ToList(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"populate\": true}]", browser()->profile()));

  EXPECT_EQ(window_ids.size(), windows.size());
  for (const base::Value& result_window : windows) {
    base::Value::Dict result_window_dict = utils::ToDict(result_window);
    result_ids.insert(GetWindowId(result_window_dict));

    // "populate" was enabled so tabs should be populated.
    const base::Value::List* tabs =
        result_window_dict.FindList(ExtensionTabUtil::kTabsKey);
    EXPECT_TRUE(tabs);
  }
  // The returned ids should contain all the current app, browser and
  // devtools instance ids.
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);

  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, GetAllWindowsAllTypes) {
  const size_t NUM_WINDOWS = 5;
  std::set<int> window_ids;
  std::set<int> result_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));

  for (size_t i = 0; i < NUM_WINDOWS - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Application windows should not be accessible to extensions (app windows are
  // only accessible to the owning item).
  AppWindow* app_window = CreateTestAppWindow("{}");

  // Undocked DevTools window should be accessible too, since they have been
  // explicitly requested as part of the type filter mask.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  window_ids.insert(ExtensionTabUtil::GetWindowId(
      DevToolsWindowTesting::Get(devtools)->browser()));

  auto function = base::MakeRefCounted<WindowsGetAllFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::Value::List windows(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(),
          "[{\"windowTypes\": [\"app\", \"devtools\", \"normal\", \"panel\", "
          "\"popup\"]}]",
          browser()->profile())));

  EXPECT_EQ(window_ids.size(), windows.size());
  for (const base::Value& result_window : windows) {
    base::Value::Dict result_window_dict = utils::ToDict(result_window);
    result_ids.insert(GetWindowId(result_window_dict));

    // "populate" was not passed in so tabs are not populated.
    const base::Value::List* tabs =
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
      browser()->profile()));

  EXPECT_EQ(window_ids.size(), windows.size());
  for (const base::Value& result_window : windows) {
    base::Value::Dict result_window_dict = utils::ToDict(result_window);
    result_ids.insert(GetWindowId(result_window_dict));

    // "populate" was enabled so tabs should be populated.
    const base::Value::List* tabs =
        result_window_dict.FindList(ExtensionTabUtil::kTabsKey);
    EXPECT_TRUE(tabs);
  }
  // The returned ids should contain all the browser and devtools instance ids.
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);

  CloseAppWindow(app_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, UpdateNoPermissions) {
  // The test empty extension has no permissions, therefore it should not get
  // tab data in the function result.
  auto update_tab_function = base::MakeRefCounted<TabsUpdateFunction>();
  scoped_refptr<const Extension> empty_extension(
      ExtensionBuilder("Test").Build());
  update_tab_function->set_extension(empty_extension.get());
  // Without a callback the function will not generate a result.
  update_tab_function->set_has_callback(true);

  const base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          update_tab_function.get(),
          "[null, {\"url\": \"about:blank\", \"pinned\": true}]",
          browser()->profile()));
  // The url is stripped since the extension does not have tab permissions.
  EXPECT_FALSE(result.contains("url"));
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "pinned"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DisallowNonIncognitoUrlInIncognitoWindow) {
  Browser* incognito = CreateIncognitoBrowser();

  auto update_tab_function = base::MakeRefCounted<TabsUpdateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  update_tab_function->set_extension(extension.get());
  update_tab_function->set_include_incognito_information(true);

  std::string error = api_test_utils::RunFunctionAndReturnError(
      update_tab_function.get(),
      std::string("[null, {\"url\": \"") + chrome::kChromeUIExtensionsURL +
          chrome::kExtensionConfigureCommandsSubPage + "\"}]",
      incognito->profile(),  // incognito doesn't have any tabs.
      api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                tabs_constants::kURLsNotAllowedInIncognitoError,
                std::string(chrome::kChromeUIExtensionsURL) +
                    chrome::kExtensionConfigureCommandsSubPage),
            error);

  // Ensure the tab was not updated. It should stay as the new tab page.
  EXPECT_EQ(1, incognito->tab_strip_model()->count());
  EXPECT_EQ(GURL(url::kAboutBlankURL), incognito->tab_strip_model()
                                           ->GetActiveWebContents()
                                           ->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DefaultToIncognitoWhenItIsForced) {
  static const char kArgsWithoutExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\"}]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  // Run without an explicit "incognito" param.
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  function->SetRenderFrameHost(browser()
                                   ->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetPrimaryMainFrame());
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), kArgsWithoutExplicitIncognitoParam,
          browser()->profile(), api_test_utils::FunctionMode::kIncognito));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()), GetWindowId(result));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  function = base::MakeRefCounted<WindowsCreateFunction>();
  function->SetRenderFrameHost(browser()
                                   ->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetPrimaryMainFrame());
  function->set_extension(extension.get());
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      function.get(), kArgsWithoutExplicitIncognitoParam,
      incognito_browser->profile(), api_test_utils::FunctionMode::kIncognito));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            GetWindowId(result));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DefaultToIncognitoWhenItIsForcedAndNoArgs) {
  static const char kEmptyArgs[] = "[]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);
  // Run without an explicit "incognito" param.
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), kEmptyArgs, browser()->profile(),
          api_test_utils::FunctionMode::kIncognito));

  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(browser()), GetWindowId(result));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "incognito"));

  // Now try creating a window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run without an explicit "incognito" param.
  function = base::MakeRefCounted<WindowsCreateFunction>();
  function->set_extension(extension.get());
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      function.get(), kEmptyArgs, incognito_browser->profile(),
      api_test_utils::FunctionMode::kIncognito));
  // Make sure it is a new(different) window.
  EXPECT_NE(ExtensionTabUtil::GetWindowId(incognito_browser),
            GetWindowId(result));
  // ... and it is incognito.
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "incognito"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DontCreateNormalWindowWhenIncognitoForced) {
  static const char kArgsWithExplicitIncognitoParam[] =
      "[{\"url\": \"about:blank\", \"incognito\": false }]";
  // Force Incognito mode.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kForced);

  // Run with an explicit "incognito" param.
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgsWithExplicitIncognitoParam,
                                       browser()->profile()),
      keys::kIncognitoModeIsForced));

  // Now try opening a normal window from incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Run with an explicit "incognito" param.
  function = base::MakeRefCounted<WindowsCreateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(function.get(),
                                       kArgsWithExplicitIncognitoParam,
                                       incognito_browser->profile()),
      keys::kIncognitoModeIsForced));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest,
                       DontCreateIncognitoWindowWhenIncognitoDisabled) {
  static const char kArgs[] =
      "[{\"url\": \"about:blank\", \"incognito\": true }]";

  Browser* incognito_browser = CreateIncognitoBrowser();
  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);
  // Run in normal window.
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  EXPECT_TRUE(
      base::MatchPattern(utils::RunFunctionAndReturnError(function.get(), kArgs,
                                                          browser()->profile()),
                         keys::kIncognitoModeIsDisabled));

  // Run in incognito window.
  function = base::MakeRefCounted<WindowsCreateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(function.get(), kArgs,
                                       incognito_browser->profile()),
      keys::kIncognitoModeIsDisabled));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryCurrentWindowTabs) {
  const size_t kExtraWindows = 3;
  for (size_t i = 0; i < kExtraWindows; ++i)
    CreateBrowser(browser()->profile());

  GURL url(url::kAboutBlankURL);
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_LINK));
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  // Get tabs in the 'current' window called from non-focused browser.
  auto function = base::MakeRefCounted<TabsQueryFunction>();
  function->set_extension(ExtensionBuilder("Test").Build().get());

  base::Value::List result_tabs =
      utils::ToList(RunFunctionWithDispatcherDelegateAndReturnValue(
          function.get(), "[{\"currentWindow\":true}]", browser()));

  // We should have one initial tab and one added tab.
  EXPECT_EQ(2u, result_tabs.size());
  for (const base::Value& result_tab : result_tabs) {
    EXPECT_EQ(window_id, GetTabWindowId(utils::ToDict(result_tab)));
  }

  // Get tabs NOT in the 'current' window called from non-focused browser.
  function = base::MakeRefCounted<TabsQueryFunction>();
  function->set_extension(ExtensionBuilder("Test").Build().get());
  result_tabs = utils::ToList(RunFunctionWithDispatcherDelegateAndReturnValue(
      function.get(), "[{\"currentWindow\":false}]", browser()));

  // We should have one tab for each extra window.
  EXPECT_EQ(kExtraWindows, result_tabs.size());
  for (const base::Value& result_tab : result_tabs) {
    EXPECT_NE(window_id, GetTabWindowId(utils::ToDict(result_tab)));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryAllTabsWithDevTools) {
  const size_t kNumWindows = 3;
  std::set<int> window_ids;
  window_ids.insert(ExtensionTabUtil::GetWindowId(browser()));
  for (size_t i = 0; i < kNumWindows - 1; ++i) {
    Browser* new_browser = CreateBrowser(browser()->profile());
    window_ids.insert(ExtensionTabUtil::GetWindowId(new_browser));
  }

  // Undocked DevTools window should not be accessible.
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  // Get tabs in the 'current' window called from non-focused browser.
  auto function = base::MakeRefCounted<TabsQueryFunction>();
  function->set_extension(ExtensionBuilder("Test").Build().get());
  base::Value::List result_tabs(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{}]", browser()->profile())));

  std::set<int> result_ids;
  // We should have one tab per browser except for DevTools.
  EXPECT_EQ(kNumWindows, result_tabs.size());
  for (const base::Value& result_tab : result_tabs) {
    result_ids.insert(GetTabWindowId(utils::ToDict(result_tab)));
  }
  EXPECT_EQ(window_ids, result_ids);

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, QueryTabGroups) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  GURL url(url::kAboutBlankURL);
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_LINK));
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  auto function = base::MakeRefCounted<TabsQueryFunction>();
  function->set_extension(ExtensionBuilder("Test").Build().get());
  constexpr char kFormatQueryArgs[] = R"([{"groupId":%d}])";
  const std::string args = base::StringPrintf(
      kFormatQueryArgs, ExtensionTabUtil::GetGroupId(group_id));
  base::Value::List result(
      utils::ToList(utils::RunFunctionAndReturnSingleResult(
          function.get(), args, browser()->profile())));

  EXPECT_EQ(2u, result.size());
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DontCreateTabInClosingPopupWindow) {
  // Test creates new popup window, closes it right away and then tries to open
  // a new tab in it. Tab should not be opened in the popup window, but in a
  // tabbed browser window.
  Browser* popup_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_POPUP, browser()->profile(), true));
  int window_id = ExtensionTabUtil::GetWindowId(popup_browser);
  chrome::CloseWindow(popup_browser);

  auto create_tab_function = base::MakeRefCounted<TabsCreateFunction>();
  create_tab_function->set_extension(ExtensionBuilder("Test").Build().get());
  // Without a callback the function will not generate a result.
  create_tab_function->set_has_callback(true);

  static const char kNewBlankTabArgs[] =
      "[{\"url\": \"about:blank\", \"windowId\": %u}]";

  const base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          create_tab_function.get(),
          base::StringPrintf(kNewBlankTabArgs, window_id),
          browser()->profile()));

  EXPECT_NE(window_id, GetTabWindowId(result));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, InvalidUpdateWindowState) {
  int window_id = ExtensionTabUtil::GetWindowId(browser());

  static const char kArgsMinimizedWithFocus[] =
      "[%u, {\"state\": \"minimized\", \"focused\": true}]";
  auto function = base::MakeRefCounted<WindowsUpdateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithFocus, window_id),
          browser()->profile()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithoutFocus[] =
      "[%u, {\"state\": \"maximized\", \"focused\": false}]";
  function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithoutFocus, window_id),
          browser()->profile()),
      keys::kInvalidWindowStateError));

  static const char kArgsMinimizedWithBounds[] =
      "[%u, {\"state\": \"minimized\", \"width\": 500}]";
  function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMinimizedWithBounds, window_id),
          browser()->profile()),
      keys::kInvalidWindowStateError));

  static const char kArgsMaximizedWithBounds[] =
      "[%u, {\"state\": \"maximized\", \"width\": 500}]";
  function = base::MakeRefCounted<WindowsUpdateFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(kArgsMaximizedWithBounds, window_id),
          browser()->profile()),
      keys::kInvalidWindowStateError));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, InvalidUpdateWindowBounds) {
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());

  // Get the display bounds so we can test whether the window intersects.
  gfx::Rect displays;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays())
    displays.Union(display.bounds());

  int window_id = ExtensionTabUtil::GetWindowId(browser());
  gfx::Rect window_bounds = browser()->window()->GetBounds();

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
            browser()->profile()),
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
            browser()->profile()),
        keys::kInvalidWindowBoundsError));
  }

  {
    // Window bounds that intersect 50% or more with the display are valid.
    int window_left = displays.right() - window_bounds.width() + window_offset;
    int window_top = displays.bottom() - window_bounds.height() + window_offset;
    auto function = base::MakeRefCounted<WindowsUpdateFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(utils::RunFunction(
        function.get(),
        base::StringPrintf(kArgsUpdateFunction, window_id, window_left,
                           window_top),
        browser()->profile(), api_test_utils::FunctionMode::kNone));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, UpdateDevToolsWindow) {
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  auto get_function = base::MakeRefCounted<WindowsGetFunction>();
  scoped_refptr<const Extension> extension(
      ExtensionBuilder("Test").Build().get());
  get_function->set_extension(extension.get());
  base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          get_function.get(),
          base::StringPrintf(
              "[%u, {\"windowTypes\": [\"devtools\"]}]",
              ExtensionTabUtil::GetWindowId(
                  DevToolsWindowTesting::Get(devtools)->browser())),
          browser()->profile()));

  // Verify the updating width/height works.
  int32_t new_width = api_test_utils::GetInteger(result, "width") - 50;
  int32_t new_height = api_test_utils::GetInteger(result, "height") - 50;

  auto update_function = base::MakeRefCounted<WindowsUpdateFunction>();
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      update_function.get(),
      base::StringPrintf("[%u, {\"width\": %d, \"height\": %d}]",
                         ExtensionTabUtil::GetWindowId(
                             DevToolsWindowTesting::Get(devtools)->browser()),
                         new_width, new_height),
      browser()->profile()));

  EXPECT_EQ(new_width, api_test_utils::GetInteger(result, "width"));
  EXPECT_EQ(new_height, api_test_utils::GetInteger(result, "height"));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, ExtensionAPICannotNavigateDevtools) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").AddAPIPermission("tabs").Build();

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  auto function = base::MakeRefCounted<TabsUpdateFunction>();
  function->set_extension(extension.get());

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(
              "[%d, {\"url\":\"http://example.com\"}]",
              ExtensionTabUtil::GetTabId(
                  DevToolsWindowTesting::Get(devtools)->main_web_contents())),
          DevToolsWindowTesting::Get(devtools)->browser()->profile()),
      tabs_constants::kNotAllowedForDevToolsError));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/836327
#define MAYBE_AcceptState DISABLED_AcceptState
#else
#define MAYBE_AcceptState AcceptState
#endif
IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, MAYBE_AcceptState) {
#if BUILDFLAG(IS_MAC)
  ui::test::ScopedFakeNSWindowFullscreen fake_fullscreen;
#endif

  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());
  function->SetBrowserContextForTesting(browser()->profile());

  base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), "[{\"state\": \"minimized\"}]", browser()->profile(),
          api_test_utils::FunctionMode::kIncognito));
  int window_id = GetWindowId(result);
  std::string error;

  WindowController* new_controller =
      ExtensionTabUtil::GetControllerFromWindowID(
          ChromeExtensionFunctionDetails(function.get()), window_id, &error);
  ASSERT_TRUE(new_controller);
  EXPECT_TRUE(error.empty());
  Browser* new_browser = new_controller->GetBrowser();
  ASSERT_TRUE(new_browser);

// TODO(crbug.com/40254339): Remove this workaround if this wait is no longer
// needed.
// These builds flags are limiting the check for IsMinimized() for Linux
// and Lacros. For Linux and Lacros we only check X11 window manager and not
// wayland since our current fix only applies to X11.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// Must be checked inside IS_LINUX to compile on windows/mac.
#if BUILDFLAG(IS_OZONE_X11)
  // DesktopWindowTreeHostX11::IsMinimized() relies on an asynchronous update
  // from the window server
  views::test::PropertyWaiter minimize_waiter(
      base::BindRepeating(&BrowserWindow::IsMinimized,
                          base::Unretained(new_browser->window())),
      true);
  EXPECT_TRUE(minimize_waiter.Wait());
#elif BUILDFLAG(IS_OZONE_WAYLAND)
  // TODO(crbug.com/40252593): Find a fix/workaround for wayland and add
  // verification of IsMinimized() for as well.
#endif
#else
  EXPECT_TRUE(new_controller->window()->IsMinimized());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

  function = base::MakeRefCounted<WindowsCreateFunction>();
  function->set_extension(extension.get());
  function->SetBrowserContextForTesting(browser()->profile());
  result = utils::ToDict(utils::RunFunctionAndReturnSingleResult(
      function.get(), "[{\"state\": \"fullscreen\"}]", browser()->profile(),
      api_test_utils::FunctionMode::kIncognito));
  window_id = GetWindowId(result);

  new_controller = ExtensionTabUtil::GetControllerFromWindowID(
      ChromeExtensionFunctionDetails(function.get()), window_id, &error);
  ASSERT_TRUE(new_controller);
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(new_controller->GetBrowser()->window()->IsFullscreen());

  // Let the message loop run so that |fake_fullscreen| finishes transition.
  content::RunAllPendingInMessageLoop();
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
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays())
    displays.Union(display.bounds());

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
        browser()->profile(), api_test_utils::FunctionMode::kNone));
  }

  {
    // Window bounds that specify size and not position should be adjusted
    // to the screen in case the window is not visible.
    // For this, update the current window bounds so the new window position
    // needs to be adjusted to fit.
    gfx::Rect current_window_bounds = browser()->window()->GetBounds();
    current_window_bounds.set_x(current_window_bounds.x() +
                                current_window_bounds.width() - window_offset);
    current_window_bounds.set_y(current_window_bounds.y() +
                                current_window_bounds.height() - window_offset);
    browser()->window()->SetBounds(current_window_bounds);

    static const char kArgsCreateFunctionOnlySize[] =
        "[{\"width\": %d, \"height\": %d }]";
    auto function = base::MakeRefCounted<WindowsCreateFunction>();
    function->set_extension(ExtensionBuilder("Test").Build().get());
    EXPECT_TRUE(utils::RunFunction(
        function.get(),
        base::StringPrintf(kArgsCreateFunctionOnlySize, window_width,
                           window_height),
        browser()->profile(), api_test_utils::FunctionMode::kNone));
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionWindowCreateTest, CreatePopupWindowFromWebUI) {
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  function->SetBrowserContextForTesting(browser()->profile());
  function->set_source_context_type(mojom::ContextType::kUntrustedWebUi);

  const base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          function.get(), R"([{"type": "popup"}])", browser()->profile()));
  int window_id = GetWindowId(result);

  std::string error;
  EXPECT_TRUE(ExtensionTabUtil::GetControllerFromWindowID(
      ChromeExtensionFunctionDetails(function.get()), window_id, &error));
  EXPECT_TRUE(error.empty());
}

struct ExtensionWindowCreateIwaParam {
  std::string test_name;
  bool want_success;
  std::string args;
};

// Test that `windows.create` functions correctly for Isolated Web Apps.
class ExtensionWindowCreateIwaTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<ExtensionWindowCreateIwaParam> {
 public:
  ExtensionWindowCreateIwaTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
    set_open_about_blank_on_browser_launch(false);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(profile()));
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    // Suppress "Welcome to Google Chrome" window
    command_line->AppendSwitch(switches::kNoFirstRun);
    command_line->AppendSwitch(switches::kNoStartupWindow);
    command_line->AppendSwitch(switches::kKeepAliveForTest);
  }

  void TearDownOnMainThread() override {
    if (BrowserList::GetInstance()->empty()) {
      // Tests crash during teardown if no browser has opened combined with the
      // command line switches above. Open a browser to avoid the crash.
      CreateBrowser(profile());
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() {
    // We cannot use `browser()->profile()` here, because `browser()` is
    // `nullptr` due to the command line switches above.
    return ProfileManager::GetLastUsedProfile();
  }

 protected:
  void InstallAndTrustBundle() {
    auto bundle = web_app::TestSignedWebBundleBuilder::BuildDefault(
        web_app::TestSignedWebBundleBuilder::BuildOptions()
            .AddKeyPair(web_app::test::GetDefaultEd25519KeyPair())
            .SetIndexHTMLContent("Hello Extensions!"));

    base::FilePath bundle_path =
        scoped_temp_dir_.GetPath().AppendASCII("bundle.swbn");
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::WriteFile(bundle_path, bundle.data));
    }

    web_app::SetTrustedWebBundleIdsForTesting({bundle.id});

    base::test::TestFuture<
        base::expected<web_app::InstallIsolatedWebAppCommandSuccess,
                       web_app::InstallIsolatedWebAppCommandError>>
        future;
    web_app::WebAppProvider::GetForTest(profile())
        ->scheduler()
        .InstallIsolatedWebApp(
            web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                bundle.id),
            web_app::IsolatedWebAppInstallSource::FromGraphicalInstaller(
                web_app::IwaSourceBundleProdModeWithFileOp(
                    bundle_path, web_app::IwaSourceBundleProdFileOp::kCopy)),
            /*expected_version=*/std::nullopt,
            /*optional_keep_alive=*/nullptr,
            /*optional_profile_keep_alive=*/nullptr, future.GetCallback());
    EXPECT_THAT(future.Take(), base::test::HasValue());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
  base::ScopedTempDir scoped_temp_dir_;
};

IN_PROC_BROWSER_TEST_P(ExtensionWindowCreateIwaTest, CreateWindowForIwa) {
  EXPECT_NO_FATAL_FAILURE(InstallAndTrustBundle());

  EXPECT_EQ(BrowserList::GetInstance()->size(), 0ul);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ExtensionWindowCreateIwaTest").Build();
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  function->set_extension(extension);
  bool result =
      api_test_utils::RunFunction(function.get(), GetParam().args, profile(),
                                  api_test_utils::FunctionMode::kNone);
  if (GetParam().want_success) {
    EXPECT_TRUE(result) << function->GetError();

    // A single browser for the IWA should now be open.
    ASSERT_EQ(BrowserList::GetInstance()->size(), 1ul);
    Browser* iwa_browser = *BrowserList::GetInstance()->begin();
    ASSERT_EQ(iwa_browser->tab_strip_model()->count(), 1);
    EXPECT_EQ(iwa_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
              GURL("isolated-app://"
                   "4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/"
                   "index.html"));
  } else {
    EXPECT_FALSE(result);
    // No browser should have opened.
    EXPECT_EQ(BrowserList::GetInstance()->size(), 0ul);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ExtensionWindowCreateIwaTest,
    testing::Values(
        ExtensionWindowCreateIwaParam{.test_name = "iwa_and_https",
                                      .want_success = false,
                                      .args = R"([{
            "url": [
              "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/index.html",
              "https://example.com"
            ]
          }])"},
        ExtensionWindowCreateIwaParam{.test_name = "https_and_iwa_and_https",
                                      .want_success = false,
                                      .args = R"([{
            "url": [
              "https://example.com",
              "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/index.html",
              "https://example.com"
            ]
          }])"},
        // If we ever support tabbed IWAs, then this test must be updated to
        // `.want_success true`.
        ExtensionWindowCreateIwaParam{.test_name = "iwa_and_iwa",
                                      .want_success = false,
                                      .args = R"([{
            "url": [
              "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/index.html",
              "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/index.html"
            ]
          }])"},
        ExtensionWindowCreateIwaParam{.test_name = "iwa_and_different_iwa",
                                      .want_success = false,
                                      .args = R"([{
            "url": [
              "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/index.html",
              "isolated-app://5dp4lo5h6tpc4vuokowxmlqs5gpbainu2nqvuddccx5mqsnje7fqaaic/index.html"
            ]
          }])"},
        ExtensionWindowCreateIwaParam{.test_name = "invalid_iwa_url",
                                      .want_success = false,
                                      .args = R"([{
            "url": [
              "isolated-app://invalid-iwa-url"
            ]
          }])"},
        // If we ever support tabbed IWAs, this test must be updated: If `tabId`
        // refers to the tab of the same IWA origin that is specified in `url`,
        // it should be allowed.
        ExtensionWindowCreateIwaParam{.test_name = "iwa_and_tab_id",
                                      .want_success = false,
                                      .args = R"([{
            "url":
            "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/index.html",
            "tabId": 1
          }])"},
        ExtensionWindowCreateIwaParam{.test_name = "iwa",
                                      .want_success = true,
                                      .args = R"([{
            "url": "isolated-app://4tkrnsmftl4ggvvdkfth3piainqragus2qbhf7rlz2a3wo3rh4wqaaic/index.html",
          }])"}),
    [](const testing::TestParamInfo<ExtensionWindowCreateIwaTest::ParamType>&
           info) { return info.param.test_name; });

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DuplicateTab) {
  content::OpenURLParams params(GURL(url::kAboutBlankURL), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents =
      browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int tab_index = -1;
  TabStripModel* tab_strip;
  ExtensionTabUtil::GetTabStripModel(web_contents, &tab_strip, &tab_index);

  auto duplicate_tab_function = base::MakeRefCounted<TabsDuplicateFunction>();
  scoped_refptr<const Extension> empty_tab_extension =
      ExtensionBuilder("Test").AddAPIPermission("tabs").Build();
  duplicate_tab_function->set_extension(empty_tab_extension.get());
  duplicate_tab_function->set_has_callback(true);

  const base::Value::Dict duplicate_result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          duplicate_tab_function.get(), base::StringPrintf("[%u]", tab_id),
          browser()->profile()));

  int duplicate_tab_id = GetTabId(duplicate_result);
  int duplicate_tab_window_id = GetTabWindowId(duplicate_result);
  int duplicate_tab_index =
      api_test_utils::GetInteger(duplicate_result, "index");
  // Duplicate tab id should be different from the original tab id.
  EXPECT_NE(tab_id, duplicate_tab_id);
  EXPECT_EQ(window_id, duplicate_tab_window_id);
  EXPECT_EQ(tab_index + 1, duplicate_tab_index);
  // The test empty tab extension has tabs permissions, therefore
  // |duplicate_result| should contain url, pendingUrl, title or faviconUrl
  // in the function result.
  EXPECT_TRUE(HasAnyPrivacySensitiveFields(duplicate_result));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DuplicateTabNoPermission) {
  content::OpenURLParams params(GURL(url::kAboutBlankURL), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents =
      browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  int window_id = ExtensionTabUtil::GetWindowIdOfTab(web_contents);
  int tab_index = -1;
  TabStripModel* tab_strip;
  ExtensionTabUtil::GetTabStripModel(web_contents, &tab_strip, &tab_index);

  auto duplicate_tab_function = base::MakeRefCounted<TabsDuplicateFunction>();
  scoped_refptr<const Extension> empty_extension(
      ExtensionBuilder("Test").Build());
  duplicate_tab_function->set_extension(empty_extension.get());
  duplicate_tab_function->set_has_callback(true);

  const base::Value::Dict duplicate_result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          duplicate_tab_function.get(), base::StringPrintf("[%u]", tab_id),
          browser()->profile()));

  int duplicate_tab_id = GetTabId(duplicate_result);
  int duplicate_tab_window_id = GetTabWindowId(duplicate_result);
  int duplicate_tab_index =
      api_test_utils::GetInteger(duplicate_result, "index");
  // Duplicate tab id should be different from the original tab id.
  EXPECT_NE(tab_id, duplicate_tab_id);
  EXPECT_EQ(window_id, duplicate_tab_window_id);
  EXPECT_EQ(tab_index + 1, duplicate_tab_index);
  // The test empty extension has no permissions, therefore |duplicate_result|
  // should not contain url, pendingUrl, title and faviconUrl in the function
  // result.
  EXPECT_FALSE(HasAnyPrivacySensitiveFields(duplicate_result));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, NoTabsEventOnDevTools) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/tabs/no_events")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  listener.Reply("stop");

  ASSERT_TRUE(catcher.GetNextResult());

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, NoTabsAppWindow) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/tabs/no_events")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");

  listener.Reply("stop");

  ASSERT_TRUE(catcher.GetNextResult());

  CloseAppWindow(app_window);
}

// Crashes on Mac/Win only.  http://crbug.com/708996
#if BUILDFLAG(IS_MAC)
#define MAYBE_FilteredEvents DISABLED_FilteredEvents
#else
#define MAYBE_FilteredEvents FilteredEvents
#endif

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, MAYBE_FilteredEvents) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/windows/events")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  AppWindow* app_window = CreateTestAppWindow(
      "{\"outerBounds\": "
      "{\"width\": 300, \"height\": 300,"
      " \"minWidth\": 200, \"minHeight\": 200,"
      " \"maxWidth\": 400, \"maxHeight\": 400}}");

  Browser* browser_window =
      Browser::Create(Browser::CreateParams(browser()->profile(), true));
  AddBlankTabAndShow(browser_window);

  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(
          browser()->tab_strip_model()->GetWebContentsAt(0),
          false /* is_docked */);

  chrome::CloseWindow(browser_window);
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
  CloseAppWindow(app_window);

  // TODO(llandwerlin): It seems creating an app window on MacOSX
  // won't create an activation event whereas it does on all other
  // platform. Disable focus event tests for now.
#if BUILDFLAG(IS_MAC)
  listener.Reply("");
#else
  listener.Reply("focus");
#endif

  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, OnBoundsChanged) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("api_test/windows/bounds")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  gfx::Rect rect = browser()->window()->GetBounds();
  rect.Inset(10);
  browser()->window()->SetBounds(rect);

  listener.Reply(base::StringPrintf(
      R"({"top": %u, "left": %u, "width": %u, "height": %u})", rect.y(),
      rect.x(), rect.width(), rect.height()));

  ASSERT_TRUE(catcher.GetNextResult());
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, WindowsCreate) {
  ASSERT_TRUE(RunExtensionTest("api_test/windows/create")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, ExecuteScriptOnDevTools) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").AddAPIPermission("tabs").Build();

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);

  auto function = base::MakeRefCounted<TabsExecuteScriptFunction>();
  function->set_extension(extension.get());

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf("[%u, {\"code\": \"true\"}]",
                             api::windows::WINDOW_ID_CURRENT),
          DevToolsWindowTesting::Get(devtools)->browser()->profile()),
      manifest_errors::kCannotAccessPageWithUrl));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

// TODO(georgesak): change this browsertest to an unittest.
IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DiscardedProperty) {
  ASSERT_TRUE(g_browser_process && g_browser_process->GetTabManager());
  resource_coordinator::TabManager* tab_manager =
      g_browser_process->GetTabManager();

  // To avoid flakes when focus changes, set the active tab strip model
  // explicitly.
  resource_coordinator::GetTabLifecycleUnitSource()
      ->SetFocusedTabStripModelForTesting(browser()->tab_strip_model());

  // Create two aditional tabs.
  content::OpenURLParams params(GURL(url::kAboutBlankURL), content::Referrer(),
                                WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents_a =
      browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  content::WebContents* web_contents_b =
      browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  // Set up query function with an extension.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  auto RunQueryFunction = [this, &extension](const char* query_info) {
    auto function = base::MakeRefCounted<TabsQueryFunction>();
    function->set_extension(extension.get());
    return utils::ToList(utils::RunFunctionAndReturnSingleResult(
        function.get(), query_info, browser()->profile()));
  };

  // Get non-discarded tabs.
  {
    base::Value::List result(RunQueryFunction("[{\"discarded\": false}]"));

    // The two created plus the default tab.
    EXPECT_EQ(3u, result.size());
  }

  // Get discarded tabs.
  {
    base::Value::List result(RunQueryFunction("[{\"discarded\": true}]"));
    EXPECT_EQ(0u, result.size());
  }

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Creates Tab object to ensure the property is correct for the extension.
  api::tabs::Tab tab_object_a = ExtensionTabUtil::CreateTabObject(
      web_contents_a, kDontScrubBehavior, nullptr, tab_strip_model, 0);
  EXPECT_FALSE(tab_object_a.discarded);

  // Discards one tab.
  EXPECT_TRUE(tab_manager->DiscardTabByExtension(web_contents_a));
  web_contents_a = tab_strip_model->GetWebContentsAt(1);

  // Make sure the property is changed accordingly after discarding the tab.
  tab_object_a = ExtensionTabUtil::CreateTabObject(
      web_contents_a, kDontScrubBehavior, nullptr, tab_strip_model, 0);
  EXPECT_TRUE(tab_object_a.discarded);

  // Get non-discarded tabs after discarding one tab.
  {
    base::Value::List result(RunQueryFunction("[{\"discarded\": false}]"));
    EXPECT_EQ(2u, result.size());
  }

  // Get discarded tabs after discarding one tab.
  {
    base::Value::List result(RunQueryFunction("[{\"discarded\": true}]"));
    EXPECT_EQ(1u, result.size());

    // Make sure the returned tab is the correct one.
    int tab_id_a = ExtensionTabUtil::GetTabId(web_contents_a);

    ASSERT_TRUE(result[0].is_dict());
    std::optional<int> id = result[0].GetDict().FindInt(extension_misc::kId);
    ASSERT_TRUE(id);

    EXPECT_EQ(tab_id_a, *id);
  }

  // Discards another created tab.
  EXPECT_TRUE(tab_manager->DiscardTabByExtension(web_contents_b));

  // Get non-discarded tabs after discarding two created tabs.
  {
    base::Value::List result(RunQueryFunction("[{\"discarded\": false}]"));
    ASSERT_EQ(1u, result.size());

    // Make sure the returned tab is the correct one.
    int tab_id_c =
        ExtensionTabUtil::GetTabId(tab_strip_model->GetWebContentsAt(0));

    ASSERT_TRUE(result[0].is_dict());
    std::optional<int> id = result[0].GetDict().FindInt(extension_misc::kId);
    ASSERT_TRUE(id);

    EXPECT_EQ(tab_id_c, *id);
  }

  // Get discarded tabs after discarding two created tabs.
  {
    base::Value::List result(RunQueryFunction("[{\"discarded\": true}]"));
    EXPECT_EQ(2u, result.size());
  }

  // Activates the first created tab.
  tab_strip_model->ActivateTabAt(1);

  // Get non-discarded tabs after activating a discarded tab.
  {
    base::Value::List result(RunQueryFunction("[{\"discarded\": false}]"));
    EXPECT_EQ(2u, result.size());
  }

  // Get discarded tabs after activating a discarded tab.
  {
    base::Value::List result(RunQueryFunction("[{\"discarded\": true}]"));
    EXPECT_EQ(1u, result.size());
  }
}

// Tests chrome.tabs.discard(tabId).
IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DiscardWithId) {
  // Create an additional tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Set up the function with an extension.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  auto discard = base::MakeRefCounted<TabsDiscardFunction>();
  discard->set_extension(extension.get());

  // Run function passing the tab id as argument.
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  const base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          discard.get(), base::StringPrintf("[%u]", tab_id),
          browser()->profile()));

  // Confirms that TabManager sees the tab as discarded.
  web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
                  web_contents)
                  ->IsDiscarded());

  // Make sure the returned tab is the one discarded and its discarded state is
  // correct.
  tab_id = ExtensionTabUtil::GetTabId(web_contents);
  EXPECT_EQ(tab_id, api_test_utils::GetInteger(result, "id"));
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "discarded"));
  // The result should be scrubbed.
  EXPECT_FALSE(result.contains("url"));

  // Tests chrome.tabs.discard(tabId) with an already discarded tab. It has to
  // return the error stating that the tab couldn't be discarded.
  auto discarded = base::MakeRefCounted<TabsDiscardFunction>();
  discarded->set_extension(extension.get());
  std::string error = utils::RunFunctionAndReturnError(
      discarded.get(), base::StringPrintf("[%u]", tab_id),
      browser()->profile());
  EXPECT_TRUE(base::MatchPattern(error, keys::kCannotDiscardTab));
}

// Tests chrome.tabs.discard(invalidId).
IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DiscardWithInvalidId) {
  // Create an additional tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Set up the function with an extension.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  auto discard = base::MakeRefCounted<TabsDiscardFunction>();
  discard->set_extension(extension.get());

  // Run function passing an invalid id as argument.
  int tab_invalid_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  tab_invalid_id = std::max(
      tab_invalid_id, ExtensionTabUtil::GetTabId(
                          browser()->tab_strip_model()->GetWebContentsAt(1)));
  tab_invalid_id++;

  std::string error = utils::RunFunctionAndReturnError(
      discard.get(), base::StringPrintf("[%u]", tab_invalid_id),
      browser()->profile());

  // Discarded state should still be false as no tab was discarded.
  EXPECT_FALSE(resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
                   browser()->tab_strip_model()->GetWebContentsAt(1))
                   ->IsDiscarded());

  // Check error message.
  EXPECT_TRUE(base::MatchPattern(error, ExtensionTabUtil::kTabNotFoundError));
}

// Tests chrome.tabs.discard().
IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, DiscardWithoutId) {
  // Create an additional tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Set up the function with an extension.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  auto discard = base::MakeRefCounted<TabsDiscardFunction>();
  discard->set_extension(extension.get());

  // Run without passing an id.
  const base::Value::Dict result =
      utils::ToDict(utils::RunFunctionAndReturnSingleResult(
          discard.get(), "[]", browser()->profile()));

  // Confirms that TabManager sees the tab as discarded.
  web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
                  web_contents)
                  ->IsDiscarded());

  // Make sure the returned tab is the one discarded and its discarded state is
  // correct.
  EXPECT_EQ(ExtensionTabUtil::GetTabId(web_contents),
            api_test_utils::GetInteger(result, "id"));
  EXPECT_TRUE(api_test_utils::GetBoolean(result, "discarded"));
  // The result should be scrubbed.
  EXPECT_FALSE(result.contains("url"));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsTest, AutoDiscardableProperty) {
  // Create two aditional tabs.
  content::OpenURLParams params(GURL(url::kAboutBlankURL), content::Referrer(),
                                WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents_a =
      browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  content::WebContents* web_contents_b =
      browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  // Creates Tab object to ensure the property is correct for the extension.
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  api::tabs::Tab tab_object_a = ExtensionTabUtil::CreateTabObject(
      web_contents_a, kDontScrubBehavior, nullptr, tab_strip_model, 0);
  EXPECT_TRUE(tab_object_a.auto_discardable);

  // Set up query and update functions with the extension.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  auto RunQueryFunction = [this, &extension](const char* query_info) {
    auto function = base::MakeRefCounted<TabsQueryFunction>();
    function->set_extension(extension.get());
    return utils::ToList(utils::RunFunctionAndReturnSingleResult(
        function.get(), query_info, browser()->profile()));
  };
  auto RunUpdateFunction = [this, &extension](std::string update_info) {
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension.get());
    return utils::ToDict(utils::RunFunctionAndReturnSingleResult(
        function.get(), update_info, browser()->profile()));
  };

  // Queries and results used.
  const char* kAutoDiscardableQueryInfo = "[{\"autoDiscardable\": true}]";
  const char* kNonAutoDiscardableQueryInfo = "[{\"autoDiscardable\": false}]";

  // Get auto-discardable tabs. Returns all since tabs are auto-discardable
  // by default.
  base::Value::List query_result = RunQueryFunction(kAutoDiscardableQueryInfo);
  EXPECT_EQ(3u, query_result.size());

  // Get non auto-discardable tabs.
  query_result = RunQueryFunction(kNonAutoDiscardableQueryInfo);
  EXPECT_EQ(0u, query_result.size());

  // Update the auto-discardable state of web contents A.
  int tab_id_a = ExtensionTabUtil::GetTabId(web_contents_a);
  base::Value::Dict update_result = RunUpdateFunction(
      base::StringPrintf("[%u, {\"autoDiscardable\": false}]", tab_id_a));
  EXPECT_EQ(tab_id_a, api_test_utils::GetInteger(update_result, "id"));
  EXPECT_FALSE(api_test_utils::GetBoolean(update_result, "autoDiscardable"));

  // Make sure the property is changed accordingly after updating the tab.
  tab_object_a = ExtensionTabUtil::CreateTabObject(
      web_contents_a, kDontScrubBehavior, nullptr, tab_strip_model, 0);
  EXPECT_FALSE(tab_object_a.auto_discardable);

  // Get auto-discardable tabs after changing the status of web contents A.
  query_result = RunQueryFunction(kAutoDiscardableQueryInfo);
  EXPECT_EQ(2u, query_result.size());

  // Get non auto-discardable tabs after changing the status of web contents A.
  query_result = RunQueryFunction(kNonAutoDiscardableQueryInfo);
  ASSERT_EQ(1u, query_result.size());

  // Make sure the returned tab is the correct one.
  ASSERT_TRUE(query_result[0].is_dict());
  std::optional<int> tab_id =
      query_result[0].GetDict().FindInt(extension_misc::kId);
  ASSERT_TRUE(tab_id);
  EXPECT_EQ(tab_id_a, *tab_id);

  // Update the auto-discardable state of web contents B.
  int tab_id_b = ExtensionTabUtil::GetTabId(web_contents_b);
  update_result = RunUpdateFunction(
      base::StringPrintf("[%u, {\"autoDiscardable\": false}]", tab_id_b));
  EXPECT_EQ(tab_id_b, api_test_utils::GetInteger(update_result, "id"));
  EXPECT_FALSE(api_test_utils::GetBoolean(update_result, "autoDiscardable"));

  // Get auto-discardable tabs after changing the status of both created tabs.
  query_result = RunQueryFunction(kAutoDiscardableQueryInfo);
  EXPECT_EQ(1u, query_result.size());

  // Make sure the returned tab is the correct one.
  ASSERT_TRUE(query_result[0].is_dict());
  std::optional<int> id_value =
      query_result[0].GetDict().FindInt(extension_misc::kId);
  ASSERT_TRUE(id_value);
  EXPECT_EQ(ExtensionTabUtil::GetTabId(tab_strip_model->GetWebContentsAt(0)),
            *id_value);

  // Get auto-discardable tabs after changing the status of both created tabs.
  query_result = RunQueryFunction(kNonAutoDiscardableQueryInfo);
  EXPECT_EQ(2u, query_result.size());

  // Resets the first tab back to auto-discardable.
  update_result = RunUpdateFunction(
      base::StringPrintf("[%u, {\"autoDiscardable\": true}]", tab_id_a));
  EXPECT_EQ(tab_id_a, api_test_utils::GetInteger(update_result, "id"));
  EXPECT_TRUE(api_test_utils::GetBoolean(update_result, "autoDiscardable"));

  // Get auto-discardable tabs after resetting the status of web contents A.
  query_result = RunQueryFunction(kAutoDiscardableQueryInfo);
  EXPECT_EQ(2u, query_result.size());

  // Get non auto-discardable tabs after resetting the status of web contents A.
  query_result = RunQueryFunction(kNonAutoDiscardableQueryInfo);
  EXPECT_EQ(1u, query_result.size());
}

// Tester class for the tabs.zoom* api functions.
class ExtensionTabsZoomTest : public ExtensionTabsTest {
 public:
  void SetUpOnMainThread() override;

  // Runs chrome.tabs.setZoom().
  bool RunSetZoom(int tab_id, double zoom_factor);

  // Runs chrome.tabs.getZoom().
  testing::AssertionResult RunGetZoom(int tab_id, double* zoom_factor);

  // Runs chrome.tabs.setZoomSettings().
  bool RunSetZoomSettings(int tab_id, const char* mode, const char* scope);

  // Runs chrome.tabs.getZoomSettings().
  testing::AssertionResult RunGetZoomSettings(int tab_id,
                                              std::string* mode,
                                              std::string* scope);

  // Runs chrome.tabs.getZoomSettings() and returns default zoom.
  testing::AssertionResult RunGetDefaultZoom(int tab_id,
                                             double* default_zoom_factor);

  // Runs chrome.tabs.setZoom(), expecting an error.
  std::string RunSetZoomExpectError(int tab_id,
                                    double zoom_factor);

  // Runs chrome.tabs.setZoomSettings(), expecting an error.
  std::string RunSetZoomSettingsExpectError(int tab_id,
                                            const char* mode,
                                            const char* scope);

  content::WebContents* OpenUrlAndWaitForLoad(const GURL& url);

 private:
  scoped_refptr<const Extension> extension_;
};

void ExtensionTabsZoomTest::SetUpOnMainThread() {
  ExtensionTabsTest::SetUpOnMainThread();
  extension_ = ExtensionBuilder("Test").Build();
}

bool ExtensionTabsZoomTest::RunSetZoom(int tab_id, double zoom_factor) {
  auto set_zoom_function = base::MakeRefCounted<TabsSetZoomFunction>();
  set_zoom_function->set_extension(extension_.get());
  set_zoom_function->set_has_callback(true);

  return utils::RunFunction(
      set_zoom_function.get(),
      base::StringPrintf("[%u, %lf]", tab_id, zoom_factor),
      browser()->profile(), api_test_utils::FunctionMode::kNone);
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetZoom(
    int tab_id,
    double* zoom_factor) {
  auto get_zoom_function = base::MakeRefCounted<TabsGetZoomFunction>();
  get_zoom_function->set_extension(extension_.get());
  get_zoom_function->set_has_callback(true);

  std::optional<base::Value> get_zoom_result =
      utils::RunFunctionAndReturnSingleResult(
          get_zoom_function.get(), base::StringPrintf("[%u]", tab_id),
          browser()->profile());

  if (!get_zoom_result)
    return testing::AssertionFailure() << "no result";

  std::optional<double> maybe_value = get_zoom_result->GetIfDouble();
  if (!maybe_value.has_value())
    return testing::AssertionFailure() << "result was not a double";

  *zoom_factor = maybe_value.value();
  return testing::AssertionSuccess();
}

bool ExtensionTabsZoomTest::RunSetZoomSettings(int tab_id,
                                               const char* mode,
                                               const char* scope) {
  auto set_zoom_settings_function =
      base::MakeRefCounted<TabsSetZoomSettingsFunction>();
  set_zoom_settings_function->set_extension(extension_.get());

  std::string args;
  if (scope) {
    args = base::StringPrintf("[%u, {\"mode\": \"%s\", \"scope\": \"%s\"}]",
                              tab_id, mode, scope);
  } else {
    args = base::StringPrintf("[%u, {\"mode\": \"%s\"}]", tab_id, mode);
  }

  return utils::RunFunction(set_zoom_settings_function.get(), args,
                            browser()->profile(),
                            api_test_utils::FunctionMode::kNone);
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetZoomSettings(
    int tab_id,
    std::string* mode,
    std::string* scope) {
  DCHECK(mode);
  DCHECK(scope);
  auto get_zoom_settings_function =
      base::MakeRefCounted<TabsGetZoomSettingsFunction>();
  get_zoom_settings_function->set_extension(extension_.get());
  get_zoom_settings_function->set_has_callback(true);

  std::optional<base::Value> get_zoom_settings_result =
      utils::RunFunctionAndReturnSingleResult(
          get_zoom_settings_function.get(), base::StringPrintf("[%u]", tab_id),
          browser()->profile());

  if (!get_zoom_settings_result)
    return testing::AssertionFailure() << "no result";

  base::Value::Dict get_zoom_settings_dict =
      utils::ToDict(std::move(get_zoom_settings_result));
  *mode = api_test_utils::GetString(get_zoom_settings_dict, "mode");
  *scope = api_test_utils::GetString(get_zoom_settings_dict, "scope");

  return testing::AssertionSuccess();
}

testing::AssertionResult ExtensionTabsZoomTest::RunGetDefaultZoom(
    int tab_id,
    double* default_zoom_factor) {
  DCHECK(default_zoom_factor);
  auto get_zoom_settings_function =
      base::MakeRefCounted<TabsGetZoomSettingsFunction>();
  get_zoom_settings_function->set_extension(extension_.get());
  get_zoom_settings_function->set_has_callback(true);

  std::optional<base::Value> get_zoom_settings_result =
      utils::RunFunctionAndReturnSingleResult(
          get_zoom_settings_function.get(), base::StringPrintf("[%u]", tab_id),
          browser()->profile());

  if (!get_zoom_settings_result && get_zoom_settings_result->is_dict()) {
    return testing::AssertionFailure()
           << "no result or result is not a dictionary";
  }

  std::optional<double> default_zoom_factor_setting =
      get_zoom_settings_result->GetDict().FindDouble("defaultZoomFactor");
  if (!default_zoom_factor_setting) {
    return testing::AssertionFailure()
           << "default zoom factor not found in result";
  }
  *default_zoom_factor = *default_zoom_factor_setting;

  return testing::AssertionSuccess();
}

std::string ExtensionTabsZoomTest::RunSetZoomExpectError(int tab_id,
                                                         double zoom_factor) {
  auto set_zoom_function = base::MakeRefCounted<TabsSetZoomFunction>();
  set_zoom_function->set_extension(extension_.get());
  set_zoom_function->set_has_callback(true);

  return utils::RunFunctionAndReturnError(
      set_zoom_function.get(),
      base::StringPrintf("[%u, %lf]", tab_id, zoom_factor),
      browser()->profile());
}

std::string ExtensionTabsZoomTest::RunSetZoomSettingsExpectError(
    int tab_id,
    const char* mode,
    const char* scope) {
  auto set_zoom_settings_function =
      base::MakeRefCounted<TabsSetZoomSettingsFunction>();
  set_zoom_settings_function->set_extension(extension_.get());

  return utils::RunFunctionAndReturnError(
      set_zoom_settings_function.get(),
      base::StringPrintf("[%u, {\"mode\": \"%s\", "
                         "\"scope\": \"%s\"}]",
                         tab_id, mode, scope),
      browser()->profile());
}

content::WebContents* ExtensionTabsZoomTest::OpenUrlAndWaitForLoad(
    const GURL& url) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  return  browser()->tab_strip_model()->GetActiveWebContents();
}

namespace {

double GetZoomLevel(const content::WebContents* web_contents) {
  return zoom::ZoomController::FromWebContents(web_contents)->GetZoomLevel();
}

content::OpenURLParams GetOpenParams(const char* url) {
  return content::OpenURLParams(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, SetAndGetZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Test default values before we set anything.
  double zoom_factor = -1;
  EXPECT_TRUE(RunGetZoom(tab_id, &zoom_factor));
  EXPECT_EQ(1.0, zoom_factor);

  // Test chrome.tabs.setZoom().
  const double kZoomLevel = 0.8;
  EXPECT_TRUE(RunSetZoom(tab_id, kZoomLevel));
  EXPECT_EQ(kZoomLevel,
            blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents)));

  // Test chrome.tabs.getZoom().
  zoom_factor = -1;
  EXPECT_TRUE(RunGetZoom(tab_id, &zoom_factor));
  EXPECT_EQ(kZoomLevel, zoom_factor);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, GetDefaultZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  double default_zoom_factor = -1.0;
  EXPECT_TRUE(RunGetDefaultZoom(tab_id, &default_zoom_factor));
  EXPECT_TRUE(blink::ZoomValuesEqual(
      zoom_controller->GetDefaultZoomLevel(),
      blink::ZoomFactorToZoomLevel(default_zoom_factor)));

  // Change the default zoom level and verify GetDefaultZoom returns the
  // correct value.
  content::StoragePartition* partition =
      web_contents->GetBrowserContext()->GetStoragePartition(
          web_contents->GetSiteInstance());
  ChromeZoomLevelPrefs* zoom_prefs =
      static_cast<ChromeZoomLevelPrefs*>(partition->GetZoomLevelDelegate());

  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();
  zoom_prefs->SetDefaultZoomLevelPref(default_zoom_level + 0.5);
  default_zoom_factor = -1.0;
  EXPECT_TRUE(RunGetDefaultZoom(tab_id, &default_zoom_factor));
  EXPECT_TRUE(blink::ZoomValuesEqual(
      default_zoom_level + 0.5,
      blink::ZoomFactorToZoomLevel(default_zoom_factor)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, SetToDefaultZoom) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  double default_zoom_level = zoom_controller->GetDefaultZoomLevel();
  double new_default_zoom_level = default_zoom_level + 0.42;

  content::StoragePartition* partition =
      web_contents->GetBrowserContext()->GetStoragePartition(
          web_contents->GetSiteInstance());
  ChromeZoomLevelPrefs* zoom_prefs =
      static_cast<ChromeZoomLevelPrefs*>(partition->GetZoomLevelDelegate());

  zoom_prefs->SetDefaultZoomLevelPref(new_default_zoom_level);

  double observed_zoom_factor = -1.0;
  EXPECT_TRUE(RunSetZoom(tab_id, 0.0));
  EXPECT_TRUE(RunGetZoom(tab_id, &observed_zoom_factor));
  EXPECT_TRUE(blink::ZoomValuesEqual(
      new_default_zoom_level,
      blink::ZoomFactorToZoomLevel(observed_zoom_factor)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, ZoomSettings) {
  // In this test we need two URLs that (1) represent real pages (i.e. they
  // load without causing an error page load), (2) have different domains, and
  // (3) are zoomable by the extension API (this last condition rules out
  // chrome:// urls). We achieve this by noting that about:blank meets these
  // requirements, allowing us to spin up an embedded http server on localhost
  // to get the other domain.
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());

  GURL url_A = http_server.GetURL("/simple.html");
  GURL url_B("about:blank");

  // Tabs A1 and A2 are navigated to the same origin, while B is navigated
  // to a different one.
  content::WebContents* web_contents_A1 = OpenUrlAndWaitForLoad(url_A);
  content::WebContents* web_contents_A2 = OpenUrlAndWaitForLoad(url_A);
  content::WebContents* web_contents_B = OpenUrlAndWaitForLoad(url_B);

  int tab_id_A1 = ExtensionTabUtil::GetTabId(web_contents_A1);
  int tab_id_A2 = ExtensionTabUtil::GetTabId(web_contents_A2);
  int tab_id_B = ExtensionTabUtil::GetTabId(web_contents_B);

  ASSERT_FLOAT_EQ(1.f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  ASSERT_FLOAT_EQ(1.f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
  ASSERT_FLOAT_EQ(1.f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_B)));

  // Test per-origin automatic zoom settings.
  EXPECT_TRUE(RunSetZoom(tab_id_B, 1.f));
  EXPECT_TRUE(RunSetZoom(tab_id_A2, 1.1f));
  EXPECT_FLOAT_EQ(1.1f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(1.1f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
  EXPECT_FLOAT_EQ(1.f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_B)));

  // Test per-tab automatic zoom settings.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "automatic", "per-tab"));
  EXPECT_TRUE(RunSetZoom(tab_id_A1, 1.2f));
  EXPECT_FLOAT_EQ(1.2f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(1.1f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));

  // Test 'manual' mode.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "manual", nullptr));
  EXPECT_TRUE(RunSetZoom(tab_id_A1, 1.3f));
  EXPECT_FLOAT_EQ(1.3f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  EXPECT_FLOAT_EQ(1.1f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));

  // Test 'disabled' mode, which will reset A1's zoom to 1.f.
  EXPECT_TRUE(RunSetZoomSettings(tab_id_A1, "disabled", nullptr));
  std::string error = RunSetZoomExpectError(tab_id_A1, 1.4f);
  EXPECT_TRUE(base::MatchPattern(error, keys::kCannotZoomDisabledTabError));
  EXPECT_FLOAT_EQ(1.f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A1)));
  // We should still be able to zoom A2 though.
  EXPECT_TRUE(RunSetZoom(tab_id_A2, 1.4f));
  EXPECT_FLOAT_EQ(1.4f,
                  blink::ZoomLevelToZoomFactor(GetZoomLevel(web_contents_A2)));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, PerTabResetsOnNavigation) {
  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());

  GURL url_A = http_server.GetURL("/simple.html");
  GURL url_B("about:blank");

  content::WebContents* web_contents = OpenUrlAndWaitForLoad(url_A);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  EXPECT_TRUE(RunSetZoomSettings(tab_id, "automatic", "per-tab"));

  std::string mode;
  std::string scope;
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-tab", scope);

  // Navigation of tab should reset mode to per-origin.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(), url_B,
                                                            1);
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-origin", scope);
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, GetZoomSettings) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  std::string mode;
  std::string scope;

  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));
  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-origin", scope);

  EXPECT_TRUE(RunSetZoomSettings(tab_id, "automatic", "per-tab"));
  EXPECT_TRUE(RunGetZoomSettings(tab_id, &mode, &scope));

  EXPECT_EQ("automatic", mode);
  EXPECT_EQ("per-tab", scope);

  std::string error =
      RunSetZoomSettingsExpectError(tab_id, "manual", "per-origin");
  EXPECT_TRUE(base::MatchPattern(error, keys::kPerOriginOnlyInAutomaticError));
  error =
      RunSetZoomSettingsExpectError(tab_id, "disabled", "per-origin");
  EXPECT_TRUE(base::MatchPattern(error, keys::kPerOriginOnlyInAutomaticError));
}

IN_PROC_BROWSER_TEST_F(ExtensionTabsZoomTest, CannotZoomInvalidTab) {
  content::OpenURLParams params(GetOpenParams(url::kAboutBlankURL));
  content::WebContents* web_contents = OpenUrlAndWaitForLoad(params.url);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  int bogus_id = tab_id + 100;
  std::string error = RunSetZoomExpectError(bogus_id, 3.14159);
  EXPECT_TRUE(base::MatchPattern(error, ExtensionTabUtil::kTabNotFoundError));

  error = RunSetZoomSettingsExpectError(bogus_id, "manual", "per-tab");
  EXPECT_TRUE(base::MatchPattern(error, ExtensionTabUtil::kTabNotFoundError));

  const char kNewTestTabArgs[] = "chrome://version";
  params = GetOpenParams(kNewTestTabArgs);
  web_contents = browser()->OpenURL(params, /*navigation_handle_callback=*/{});
  tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Test chrome.tabs.setZoom().
  error = RunSetZoomExpectError(tab_id, 3.14159);
  EXPECT_TRUE(
      base::MatchPattern(error, manifest_errors::kCannotAccessChromeUrl));

  // chrome.tabs.setZoomSettings().
  error = RunSetZoomSettingsExpectError(tab_id, "manual", "per-tab");
  EXPECT_TRUE(
      base::MatchPattern(error, manifest_errors::kCannotAccessChromeUrl));
}

#if BUILDFLAG(ENABLE_PDF)
class ExtensionApiPdfTest : public base::test::WithFeatureOverride,
                            public PDFExtensionTestBase {
 public:
  ExtensionApiPdfTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  bool UseOopif() const override { return GetParam(); }
};

// Regression test for crbug.com/660498.
IN_PROC_BROWSER_TEST_P(ExtensionApiPdfTest, TemporaryAddressSpoof) {
  content::WebContents* first_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(first_web_contents);
  chrome::NewTab(browser());
  content::WebContents* second_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(first_web_contents, second_web_contents);
  GURL url = embedded_test_server()->GetURL(
      "/extensions/api_test/tabs/pdf_extension_test.html");
  content::TestNavigationManager navigation_manager(
      second_web_contents, GURL("http://www.facebook.com:83"));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Ensure the PDF has loaded, and get the WebContents to click.
  auto* web_contents_for_click = second_web_contents;
  if (UseOopif()) {
    ASSERT_TRUE(GetTestPdfViewerStreamManager(second_web_contents)
                    ->WaitUntilPdfLoadedInFirstChild());
  } else {
    ASSERT_TRUE(
        pdf_extension_test_util::EnsurePDFHasLoaded(second_web_contents));

    auto inner_web_contents = web_contents_for_click->GetInnerWebContents();
    ASSERT_EQ(1U, inner_web_contents.size());
    // With MimeHandlerViewInCrossProcessFrame input should directly route to
    // the guest WebContents as there is no longer a BrowserPlugin involved.
    web_contents_for_click = inner_web_contents[0];
  }

  // (400, 300) in `web_contents_for_click` translates to a different coordinate
  // in the PDF Viewer. The exact coordinate depends on the PDF Viewer's UI
  // layout. In the test PDF embedded in pdf_extension_test.html, the entire PDF
  // content area is a giant link to http://www.facebook.com:83. As long as this
  // click hits that link target, it triggers the navigation required for test.
  content::SimulateMouseClickAt(web_contents_for_click, 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(400, 300));

  ASSERT_TRUE(navigation_manager.WaitForRequestStart());

  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(first_web_contents,
            browser()->tab_strip_model()->GetActiveWebContents());
  browser()->tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(second_web_contents,
            browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_EQ(url, second_web_contents->GetVisibleURL());

  // Wait for the TestNavigationManager-monitored navigation to complete to
  // avoid a race during browser teardown (see crbug.com/882213).
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ExtensionApiPdfTest);
#endif  // BUILDFLAG(ENABLE_PDF)

// Tests how chrome.windows.create behaves when setSelfAsOpener parameter is
// used.  setSelfAsOpener was introduced as a fix for https://crbug.com/713888
// and https://crbug.com/718489.  This is a (slightly morphed) regression test
// for https://crbug.com/597750.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowsCreate_WithOpener) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("../simple_with_file"));
  ASSERT_TRUE(extension);

  // Navigate a tab to an extension page.
  GURL extension_url = extension->GetResourceURL("file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  content::WebContents* old_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Execute chrome.windows.create and store the new tab in |new_contents|.
  content::WebContents* new_contents = nullptr;
  {
    content::WebContentsAddedObserver observer;
    std::string script = base::StringPrintf(
        R"( window.name = 'old-contents';
            chrome.windows.create({url: '%s', setSelfAsOpener: true}); )",
        extension_url.spec().c_str());
    ASSERT_TRUE(content::ExecJs(old_contents, script));
    new_contents = observer.GetWebContents();
    ASSERT_TRUE(content::WaitForLoadStop(new_contents));
  }

  // Navigate the old and the new tab to a web URL.
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL web_url1 = embedded_test_server()->GetURL("/title1.html");
  GURL web_url2 = embedded_test_server()->GetURL("/title2.html");
  {
    content::TestNavigationObserver nav_observer(new_contents, 1);
    ASSERT_TRUE(content::ExecJs(
        new_contents, "window.location = '" + web_url1.spec() + "';"));
    nav_observer.Wait();
  }
  {
    content::TestNavigationObserver nav_observer(old_contents, 1);
    ASSERT_TRUE(content::ExecJs(
        old_contents, "window.location = '" + web_url2.spec() + "';"));
    nav_observer.Wait();
  }
  EXPECT_EQ(web_url1,
            new_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(web_url2,
            old_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Verify that the old and new tab are in the same process.
  EXPECT_EQ(old_contents->GetPrimaryMainFrame()->GetProcess(),
            new_contents->GetPrimaryMainFrame()->GetProcess());

  // Verify the old and new contents are in the same BrowsingInstance.
  EXPECT_TRUE(old_contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->IsRelatedSiteInstance(
                      new_contents->GetPrimaryMainFrame()->GetSiteInstance()));

  // Verify that the |new_contents| has |window.opener| set.
  EXPECT_EQ(old_contents->GetPrimaryMainFrame()->GetLastCommittedURL().spec(),
            EvalJs(new_contents, "window.opener.location.href"));

  // Verify that |new_contents| can find |old_contents| using window.open/name.
  std::string location_of_other_window =
      EvalJs(new_contents,
             "var w = window.open('', 'old-contents');\n"
             "w.location.href;")
          .ExtractString();
  EXPECT_EQ(old_contents->GetPrimaryMainFrame()->GetLastCommittedURL().spec(),
            location_of_other_window);
}

// Tests how chrome.windows.create behaves when setSelfAsOpener parameter is not
// used.  setSelfAsOpener was introduced as a fix for https://crbug.com/713888
// and https://crbug.com/718489.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowsCreate_NoOpener) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("../simple_with_file"));
  ASSERT_TRUE(extension);

  // Navigate a tab to an extension page.
  GURL extension_url = extension->GetResourceURL("file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  content::WebContents* old_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Execute chrome.windows.create and store the new tab in |new_contents|.
  content::WebContents* new_contents = nullptr;
  {
    content::WebContentsAddedObserver observer;
    std::string script = base::StringPrintf(
        R"( window.name = 'old-contents';
            chrome.windows.create({url: '%s'}); )",
        extension_url.spec().c_str());
    ASSERT_TRUE(content::ExecJs(old_contents, script));
    new_contents = observer.GetWebContents();
    ASSERT_TRUE(content::WaitForLoadStop(new_contents));
  }

  // Verify the old and new contents are NOT in the same BrowsingInstance.
  EXPECT_FALSE(old_contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->IsRelatedSiteInstance(
                       new_contents->GetPrimaryMainFrame()->GetSiteInstance()));

  // Verify that the |new_contents| doesn't have |window.opener| set.
  EXPECT_EQ(false, EvalJs(new_contents, "!!window.opener"));

  // TODO(lukasza): http://crbug.com/786411: Verify that |new_contents| can NOT
  // find |old_contents| using window.open/name.  This is currently broken,
  // because browsing instance boundaries are pierced for all extension frames
  // (we hope this can be limited to background pages / contents).
}

// Tests the origin of tabs created through chrome.windows.create().
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowsCreate_OpenerAndOrigin) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("../simple_with_file"));
  ASSERT_TRUE(extension);

  // Navigate a tab to an extension page.
  GURL extension_url = extension->GetResourceURL("file.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

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
  } test_cases[] = {
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
  };

  auto run_test_case = [&web_contents](const TestCase& test_case) {
    std::string maybe_specify_set_self_as_opener;
    if (test_case.set_self_as_opener) {
      maybe_specify_set_self_as_opener =
          base::StringPrintf(", setSelfAsOpener: %s",
                             *test_case.set_self_as_opener ? "true" : "false");
    }
    std::string script = base::StringPrintf(
        R"( chrome.windows.create({url: '%s'%s}); )", test_case.url.c_str(),
        maybe_specify_set_self_as_opener.c_str());

    content::WebContents* new_contents = nullptr;
    {
      content::WebContentsAddedObserver observer;
      ASSERT_TRUE(content::ExecJs(web_contents, script));
      new_contents = observer.GetWebContents();
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

// Tests updating a URL of a web tab to an about:blank.  Verify that the new
// frame is placed in the correct process, has the correct origin and that no
// DCHECKs are hit anywhere.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsUpdate_WebToAboutBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("../simple_with_file"));
  ASSERT_TRUE(extension);
  GURL extension_url = extension->GetResourceURL("file.html");
  url::Origin extension_origin = url::Origin::Create(extension_url);
  GURL web_url = embedded_test_server()->GetURL("/title1.html");
  url::Origin web_origin = url::Origin::Create(web_url);
  GURL about_blank_url = GURL(url::kAboutBlankURL);

  // Navigate a tab to an extension page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  content::WebContents* extension_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(
      extension_origin,
      extension_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // Create another tab and navigate it to a web page.
  content::WebContents* test_contents = nullptr;
  {
    content::WebContentsAddedObserver test_contents_observer;
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), web_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    test_contents = test_contents_observer.GetWebContents();
  }
  EXPECT_EQ(web_origin,
            test_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_NE(extension_contents->GetPrimaryMainFrame()->GetProcess(),
            test_contents->GetPrimaryMainFrame()->GetProcess());

  // Use |chrome.tabs.update| API to navigate |test_contents| to an about:blank
  // URL.
  {
    content::TestNavigationObserver nav_observer(test_contents, 1);
    int test_tab_id = ExtensionTabUtil::GetTabId(test_contents);
    content::ExecuteScriptAsync(
        extension_contents,
        content::JsReplace("chrome.tabs.update($1, { url: $2 })", test_tab_id,
                           about_blank_url));
    nav_observer.WaitForNavigationFinished();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(about_blank_url, nav_observer.last_navigation_url());
  }

  // Verify the origin and process of the about:blank tab.
  content::RenderFrameHost* test_frame = test_contents->GetPrimaryMainFrame();
  EXPECT_EQ(about_blank_url, test_frame->GetLastCommittedURL());
  EXPECT_EQ(extension_contents->GetPrimaryMainFrame()->GetProcess(),
            test_contents->GetPrimaryMainFrame()->GetProcess());
  // Note that committing with the extension origin wouldn't be possible when
  // targeting an incognito window (see also IncognitoApiTest.Incognito test).
  EXPECT_EQ(extension_origin, test_frame->GetLastCommittedOrigin());
}

// Tests updating a URL of a web tab to an about:newtab.  Verify that the new
// frame is placed in the correct process, has the correct origin and that no
// DCHECKs are hit anywhere.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsUpdate_WebToAboutNewTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("../simple_with_file"));
  ASSERT_TRUE(extension);
  GURL extension_url = extension->GetResourceURL("file.html");
  url::Origin extension_origin = url::Origin::Create(extension_url);
  GURL web_url = embedded_test_server()->GetURL("/title1.html");
  url::Origin web_origin = url::Origin::Create(web_url);

  // https://crbug.com/1145381: about:version is rewritten to chrome://version
  // when entered in the omnibox or used in a bookmark.  Such rewriting is
  // definitely undesirable for http-initiated navigations (see r818969), but
  // it is less clear what should happen in extension-initiated navigations.
  GURL about_newtab_url = GURL("about:newtab");
  GURL chrome_newtab_url = GURL("chrome://new-tab-page/");

  // Navigate a tab to an extension page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  content::WebContents* extension_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(
      extension_origin,
      extension_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // Create another tab and navigate it to a web page.
  content::WebContents* test_contents = nullptr;
  {
    content::WebContentsAddedObserver test_contents_observer;
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), web_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    test_contents = test_contents_observer.GetWebContents();
  }

  // Use |chrome.tabs.update| API to navigate |test_contents| to an about:newtab
  // URL.
  {
    content::TestNavigationObserver nav_observer(test_contents, 1);
    int test_tab_id = ExtensionTabUtil::GetTabId(test_contents);
    content::ExecuteScriptAsync(
        extension_contents,
        content::JsReplace("chrome.tabs.update($1, { url: $2 })", test_tab_id,
                           about_newtab_url));
    nav_observer.WaitForNavigationFinished();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(chrome_newtab_url, nav_observer.last_navigation_url());
  }

  // Verify the origin and process of the about:newtab tab.
  content::RenderFrameHost* test_frame = test_contents->GetPrimaryMainFrame();
  EXPECT_EQ(chrome_newtab_url, test_frame->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(chrome_newtab_url),
            test_frame->GetLastCommittedOrigin());
  EXPECT_NE(extension_contents->GetPrimaryMainFrame()->GetProcess(),
            test_contents->GetPrimaryMainFrame()->GetProcess());
}

// Tests updating a URL of a web tab to a non-web-accessible-resource of an
// extension - such navigation should be allowed.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, TabsUpdate_WebToNonWAR) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("../simple_with_file"));
  ASSERT_TRUE(extension);
  GURL extension_url = extension->GetResourceURL("file.html");
  url::Origin extension_origin = url::Origin::Create(extension_url);
  GURL web_url = embedded_test_server()->GetURL("/title1.html");
  url::Origin web_origin = url::Origin::Create(web_url);
  GURL non_war_url = extension_url;

  // Navigate a tab to an extension page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  content::WebContents* extension_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(
      extension_origin,
      extension_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // Create another tab and navigate it to a web page.
  content::WebContents* test_contents = nullptr;
  {
    content::WebContentsAddedObserver test_contents_observer;
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), web_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    test_contents = test_contents_observer.GetWebContents();
  }
  EXPECT_EQ(web_origin,
            test_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_NE(extension_contents->GetPrimaryMainFrame()->GetProcess(),
            test_contents->GetPrimaryMainFrame()->GetProcess());

  // Use |chrome.tabs.update| API to navigate |test_contents| to a
  // non-web-accessible-resource of an extension.
  {
    content::TestNavigationObserver nav_observer(test_contents, 1);
    int test_tab_id = ExtensionTabUtil::GetTabId(test_contents);
    content::ExecuteScriptAsync(
        extension_contents,
        content::JsReplace("chrome.tabs.update($1, { url: $2 })", test_tab_id,
                           non_war_url));
    nav_observer.WaitForNavigationFinished();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(non_war_url, nav_observer.last_navigation_url());
  }

  // Verify the origin and process of the navigated tab.
  content::RenderFrameHost* test_frame = test_contents->GetPrimaryMainFrame();
  EXPECT_EQ(non_war_url, test_frame->GetLastCommittedURL());
  EXPECT_EQ(extension_origin, test_frame->GetLastCommittedOrigin());
  EXPECT_EQ(extension_contents->GetPrimaryMainFrame()->GetProcess(),
            test_contents->GetPrimaryMainFrame()->GetProcess());
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ExtensionAPICannotCreateWindowForDevtools) {
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  auto function = base::MakeRefCounted<WindowsCreateFunction>();
  scoped_refptr<const Extension> extension(ExtensionBuilder("Test").Build());
  function->set_extension(extension.get());

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(
              R"([{"tabId": %d}])",
              ExtensionTabUtil::GetTabId(
                  DevToolsWindowTesting::Get(devtools)->main_web_contents())),
          DevToolsWindowTesting::Get(devtools)->browser()->profile()),
      tabs_constants::kNotAllowedForDevToolsError));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExtensionAPICannotMoveDevtoolsTab) {
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  auto function = base::MakeRefCounted<TabsMoveFunction>();

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(
              R"([%d, {"index": -1}])",
              ExtensionTabUtil::GetTabId(
                  DevToolsWindowTesting::Get(devtools)->main_web_contents())),
          DevToolsWindowTesting::Get(devtools)->browser()->profile()),
      tabs_constants::kNotAllowedForDevToolsError));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExtensionAPICannotGroupDevtoolsTab) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  auto function = base::MakeRefCounted<TabsGroupFunction>();

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(
              R"([{"tabIds": %d}])",
              ExtensionTabUtil::GetTabId(
                  DevToolsWindowTesting::Get(devtools)->main_web_contents())),
          DevToolsWindowTesting::Get(devtools)->browser()->profile()),
      tabs_constants::kNotAllowedForDevToolsError));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExtensionAPICannotDiscardDevtoolsTab) {
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser()->tab_strip_model()->GetWebContentsAt(0), false /* is_docked */);
  auto function = base::MakeRefCounted<TabsDiscardFunction>();

  EXPECT_TRUE(base::MatchPattern(
      utils::RunFunctionAndReturnError(
          function.get(),
          base::StringPrintf(
              "[%d]",
              ExtensionTabUtil::GetTabId(
                  DevToolsWindowTesting::Get(devtools)->main_web_contents())),
          DevToolsWindowTesting::Get(devtools)->browser()->profile()),
      tabs_constants::kNotAllowedForDevToolsError));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

}  // namespace extensions
