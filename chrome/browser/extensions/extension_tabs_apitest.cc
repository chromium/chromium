// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "net/dns/mock_host_resolver.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

class ExtensionApiTabTest : public extensions::ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

#if defined(USE_AURA) || defined(OS_MACOSX)
// Maximizing/fullscreen popup window doesn't work on aura's managed mode.
// See bug crbug.com/116305.
// Mac: http://crbug.com/103912
#define MAYBE_UpdateWindowShowState DISABLED_UpdateWindowShowState
#else
#define MAYBE_UpdateWindowShowState UpdateWindowShowState
#endif  // defined(USE_AURA) || defined(OS_MACOSX)

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
#if defined(OS_CHROMEOS)
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

  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabAudible) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "audible.html")) << message_;
}

// http://crbug.com/521410
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabMuted) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "muted.html")) << message_;
}

// Flaky on windows: http://crbug.com/238667
#if defined(OS_WIN)
#define MAYBE_Tabs2 DISABLED_Tabs2
#else
#define MAYBE_Tabs2 Tabs2
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_Tabs2) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crud2.html")) << message_;
}

// crbug.com/149924
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabDuplicate) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "duplicate.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabSize) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "tab_size.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabUpdate) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "update.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabPinned) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "pinned.html")) << message_;
}

// Flaky on windows: http://crbug.com/238667
#if defined(OS_WIN)
#define MAYBE_TabMove DISABLED_TabMove
#else
#define MAYBE_TabMove TabMove
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_TabMove) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "move.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabEvents) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "events.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabRelativeURLs) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "relative_urls.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabQuery) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "query.html")) << message_;
}

// Flaky on windows: http://crbug.com/239022
#if defined(OS_WIN)
#define MAYBE_TabHighlight DISABLED_TabHighlight
#else
#define MAYBE_TabHighlight TabHighlight
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_TabHighlight) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "highlight.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabCrashBrowser) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "crash.html")) << message_;
}

// Flaky on windows: http://crbug.com/238667
#if defined(OS_WIN)
#define MAYBE_TabOpener DISABLED_TabOpener
#else
#define MAYBE_TabOpener TabOpener
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_TabOpener) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "opener.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabGetCurrent) {
  ASSERT_TRUE(RunExtensionTest("tabs/get_current")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabConnect) {
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}

// Possible race in ChromeURLDataManager. http://crbug.com/59198
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabOnRemoved) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_removed")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_TabReload) {
  ASSERT_TRUE(RunExtensionTest("tabs/reload")) << message_;
}

class ExtensionApiCaptureTest : public ExtensionApiTabTest {
 public:
  ExtensionApiCaptureTest() {}

  void SetUp() override {
    EnablePixelOutput();
    ExtensionApiTabTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest,
                       DISABLED_CaptureVisibleTabJpeg) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_jpeg.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest, DISABLED_CaptureVisibleTabPng) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_png.html")) << message_;
}

// Times out on non-Windows.
// See http://crbug.com/80212
IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest,
                       DISABLED_CaptureVisibleTabRace) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_race.html")) << message_;
}


// Disabled for being flaky, see http://crbug/367695.
IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest,
                       DISABLED_CaptureVisibleFile) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_file.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest, CaptureVisibleDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots,
                                               true);
  ASSERT_TRUE(RunExtensionSubtest("tabs/capture_visible_tab",
                                  "test_disabled.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiCaptureTest, CaptureNullWindow) {
  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab_null_window"))
      << message_;
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

// Flaky on Linux. http://crbug.com/657376.
#if defined(OS_LINUX)
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
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
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

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_GetViewsOfCreatedPopup) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_popup.html"))
      << message_;
}
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, DISABLED_GetViewsOfCreatedWindow) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "get_views_window.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, OnUpdatedDiscardedState) {
  ASSERT_TRUE(RunExtensionSubtest("tabs/basics", "discarded.html")) << message_;
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

  EXPECT_TRUE(RunExtensionSubtestWithArgAndFlags(
      "tabs/basics", "incognito.html", args.data(),
      is_incognito_enabled ? extensions::ExtensionApiTest::kFlagEnableIncognito
                           : extensions::ExtensionApiTest::kFlagNone))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(, IncognitoExtensionApiTabTest, testing::Bool());

// Adding a new test? Awesome. But API tests are the old hotness. The new
// hotness is extension_function_test_utils. See tabs_test.cc for an example.
// We are trying to phase out many uses of API tests as they tend to be flaky.
