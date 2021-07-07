// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

class ExtensionApiTabTest : public extensions::ExtensionApiTest {
 public:
  ExtensionApiTabTest() = default;
  ~ExtensionApiTabTest() override = default;
  ExtensionApiTabTest(const ExtensionApiTabTest&) = delete;
  ExtensionApiTabTest& operator=(const ExtensionApiTabTest&) = delete;

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

class ExtensionApiTabBackForwardCacheTest : public ExtensionApiTabTest {
 public:
  ExtensionApiTabBackForwardCacheTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"content_injection_supported", "true"},
           {"all_extensions_allowed", "true"}}}},
        {features::kBackForwardCacheMemoryControls});
  }
  ~ExtensionApiTabBackForwardCacheTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ExtensionApiNewTabTest : public ExtensionApiTabTest {
 public:
  ExtensionApiNewTabTest() {}
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTabTest::SetUpCommandLine(command_line);
    // Override the default which InProcessBrowserTest adds if it doesn't see a
    // homepage.
    command_line->AppendSwitchASCII(
        switches::kHomePage, chrome::kChromeUINewTabURL);
  }
};

// Flaky on chromeos: http://crbug.com/870322
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_Tabs DISABLED_Tabs
#else
#define MAYBE_Tabs Tabs
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiNewTabTest, MAYBE_Tabs) {
  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "crud.html"}))
      << message_;
}

// TODO(crbug.com/1177118) Re-enable test
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabAudible) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "audible.html"}))
      << message_;
}

// http://crbug.com/521410
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabMuted) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "muted.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, Tabs2) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "crud2.html"}))
      << message_;
}

// crbug.com/149924
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabDuplicate) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "duplicate.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabSize) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "tab_size.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabUpdate) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "update.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabPinned) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "pinned.html"}))
      << message_;
}

// TODO(crbug.com/1227134): Flaky test.
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabMove) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "move.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabEvents) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "events.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabRelativeURLs) {
  ASSERT_TRUE(
      RunExtensionTest("tabs/basics", {.page_url = "relative_urls.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabQuery) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "query.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabHighlight) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "highlight.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabCrashBrowser) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "crash.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabOpener) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "opener.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabRemove) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "remove.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabRemoveMultiple) {
  ASSERT_TRUE(
      RunExtensionTest("tabs/basics", {.page_url = "remove-multiple.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabGetCurrent) {
  ASSERT_TRUE(RunExtensionTest("tabs/get_current")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabConnect) {
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}

// TODO(crbug.com/1222122): Flaky
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabOnRemoved) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_removed")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabReload) {
  ASSERT_TRUE(RunExtensionTest("tabs/reload")) << message_;
}

using ContextType = extensions::ExtensionBrowserTest::ContextType;

class ExtensionApiCaptureTest
    : public ExtensionApiTabTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionApiCaptureTest() = default;
  ~ExtensionApiCaptureTest() override = default;
  ExtensionApiCaptureTest(const ExtensionApiCaptureTest&) = delete;
  ExtensionApiCaptureTest& operator=(const ExtensionApiCaptureTest&) = delete;

  void SetUp() override {
    extensions::TabsCaptureVisibleTabFunction::set_disable_throttling_for_tests(
        true);
    EnablePixelOutput();
    ExtensionApiTabTest::SetUp();
  }

 protected:
  bool RunTest(const char* name,
               LoadOptions load_options = {}) WARN_UNUSED_RESULT {
    load_options.load_as_service_worker =
        GetParam() == ContextType::kServiceWorker;
    return RunExtensionTest(name, {}, load_options);
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionApiCaptureTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionApiCaptureTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, CaptureVisibleTabJpeg) {
  ASSERT_TRUE(RunTest("tabs/capture_visible_tab/test_jpeg")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, CaptureVisibleTabPng) {
  ASSERT_TRUE(RunTest("tabs/capture_visible_tab/test_png")) << message_;
}

// TODO(crbug.com/1177118) Re-enable test
IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest,
                       DISABLED_CaptureVisibleTabRace) {
  ASSERT_TRUE(RunTest("tabs/capture_visible_tab/test_race")) << message_;
}

// https://crbug.com/1107934 Flaky on Windows, Linux, ChromeOS.
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_CHROMEOS)
#define MAYBE_CaptureVisibleFile DISABLED_CaptureVisibleFile
#else
#define MAYBE_CaptureVisibleFile CaptureVisibleFile
#endif
IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, MAYBE_CaptureVisibleFile) {
  ASSERT_TRUE(RunTest("tabs/capture_visible_tab/test_file",
                      {.allow_file_access = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, CaptureVisibleDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots,
                                               true);
  ASSERT_TRUE(RunTest("tabs/capture_visible_tab/test_disabled")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, CaptureNullWindow) {
  ASSERT_TRUE(RunTest("tabs/capture_visible_tab_null_window")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabsOnCreated) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_created")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, LazyBackgroundTabsOnCreated) {
  ASSERT_TRUE(RunExtensionTest("tabs/lazy_background_on_created")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabsOnUpdated) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_updated")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabBackForwardCacheTest, TabsOnUpdated) {
  ASSERT_TRUE(RunExtensionTest("tabs/backForwardCache/on_updated")) << message_;
}

// Flaky on Linux. http://crbug.com/657376.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_TabsNoPermissions DISABLED_TabsNoPermissions
#else
#define MAYBE_TabsNoPermissions TabsNoPermissions
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_TabsNoPermissions) {
  ASSERT_TRUE(RunExtensionTest("tabs/no_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, HostPermission) {
  ASSERT_TRUE(RunExtensionTest("tabs/host_permission")) << message_;
}

// Flaky on Windows, Mac and Linux. http://crbug.com/820110.
#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#define MAYBE_UpdateWindowResize DISABLED_UpdateWindowResize
#else
#define MAYBE_UpdateWindowResize UpdateWindowResize
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_UpdateWindowResize) {
  ASSERT_TRUE(RunExtensionTest("window_update/resize")) << message_;
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, FocusWindowDoesNotUnmaximize) {
  HWND window =
      browser()->window()->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  ::SendMessage(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_TRUE(::IsZoomed(window));
}
#endif  // OS_WIN

#if defined(USE_AURA) || defined(OS_MAC)
// Maximizing/fullscreen popup window doesn't work on aura's managed mode.
// See bug crbug.com/116305.
// Mac: http://crbug.com/103912
#define MAYBE_UpdateWindowShowState DISABLED_UpdateWindowShowState
#else
#define MAYBE_UpdateWindowShowState UpdateWindowShowState
#endif  // defined(USE_AURA) || defined(OS_MAC)
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_UpdateWindowShowState) {
  ASSERT_TRUE(RunExtensionTest("window_update/show_state")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, IncognitoDisabledByPref) {
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);

  // This makes sure that creating an incognito window fails due to pref
  // (policy) being set.
  ASSERT_TRUE(RunExtensionTest("tabs/incognito_disabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, GetViewsOfCreatedPopup) {
  ASSERT_TRUE(
      RunExtensionTest("tabs/basics", {.page_url = "get_views_popup.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, GetViewsOfCreatedWindow) {
  ASSERT_TRUE(
      RunExtensionTest("tabs/basics", {.page_url = "get_views_window.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, OnUpdatedDiscardedState) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.page_url = "discarded.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabOpenerCraziness) {
  ASSERT_TRUE(RunExtensionTest("tabs/tab_opener_id"));
}

class IncognitoExtensionApiTabTest : public ExtensionApiTabTest,
                                     public testing::WithParamInterface<bool> {
};

IN_PROC_BROWSER_TEST_P(IncognitoExtensionApiTabTest, Tabs) {
  bool is_incognito_enabled = GetParam();
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  std::string args = base::StringPrintf(
      R"({"isIncognito": %s, "windowId": %d})",
      is_incognito_enabled ? "true" : "false",
      extensions::ExtensionTabUtil::GetWindowId(incognito_browser));

  EXPECT_TRUE(RunExtensionTest(
      "tabs/basics", {.page_url = "incognito.html", .custom_arg = args.c_str()},
      {.allow_in_incognito = is_incognito_enabled}))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(All, IncognitoExtensionApiTabTest, testing::Bool());

// Adding a new test? Awesome. But API tests are the old hotness. The new
// hotness is extension_function_test_utils. See tabs_test.cc for an example.
// We are trying to phase out many uses of API tests as they tend to be flaky.
