// Copyright 2012 The Chromium Authors
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
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

using ContextType = extensions::ExtensionBrowserTest::ContextType;

class ExtensionApiTabTest : public extensions::ExtensionApiTest {
 public:
  explicit ExtensionApiTabTest(ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~ExtensionApiTabTest() override = default;
  ExtensionApiTabTest(const ExtensionApiTabTest&) = delete;
  ExtensionApiTabTest& operator=(const ExtensionApiTabTest&) = delete;

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

class ExtensionApiTabTestWithContextType
    : public ExtensionApiTabTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionApiTabTestWithContextType() : ExtensionApiTabTest(GetParam()) {}
  ExtensionApiTabTestWithContextType(
      const ExtensionApiTabTestWithContextType&) = delete;
  ExtensionApiTabTestWithContextType& operator=(
      const ExtensionApiTabTestWithContextType&) = delete;
  ~ExtensionApiTabTestWithContextType() override = default;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionApiTabTestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionApiTabTestWithContextType,
                         ::testing::Values(ContextType::kServiceWorker));

class ExtensionApiTabBackForwardCacheTest
    : public ExtensionApiTabTestWithContextType {
 public:
  ExtensionApiTabBackForwardCacheTest() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetBasicBackForwardCacheFeatureForTesting(
            {{features::kBackForwardCache, {}}}),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~ExtensionApiTabBackForwardCacheTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionApiTabBackForwardCacheTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionApiTabBackForwardCacheTest,
                         ::testing::Values(ContextType::kServiceWorker));

class ExtensionApiNewTabTest : public ExtensionApiTabTestWithContextType {
 public:
  ExtensionApiNewTabTest() = default;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTabTest::SetUpCommandLine(command_line);
    // Override the default which InProcessBrowserTest adds if it doesn't see a
    // homepage.
    command_line->AppendSwitchASCII(
        switches::kHomePage, chrome::kChromeUINewTabURL);
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionApiNewTabTest,
                         ::testing::Values(ContextType::kPersistentBackground));

INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionApiNewTabTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionApiNewTabTest, Tabs) {
  // The test creates a tab and checks that the URL of the new tab
  // is that of the new tab page.  Make sure the pref that controls
  // this is set.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kHomePageIsNewTabPage, true);

  ASSERT_TRUE(RunExtensionTest("tabs/basics/crud")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, TabAudible) {
  ASSERT_TRUE(
      RunExtensionTest("tabs/basics", {.extension_url = "audible.html"}))
      << message_;
}

// TODO(crbug.com/40925613): Re-enable this test
#if BUILDFLAG(IS_MAC)
#define MAYBE_Muted DISABLED_Muted
#else
#define MAYBE_Muted Muted
#endif
IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, MAYBE_Muted) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/muted")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, Tabs2) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics", {.extension_url = "crud2.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Duplicate) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/duplicate")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Size) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/tab_size")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Update) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/update")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Pinned) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/pinned")) << message_;
}

// Flakes reported on Linux debug and Mac, see crbug.com/40936001.
#if (BUILDFLAG(IS_LINUX) && !defined(NDEBUG)) || BUILDFLAG(IS_MAC)
#define MAYBE_Move DISABLED_Move
#else
#define MAYBE_Move Move
#endif
IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, MAYBE_Move) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/move")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Events) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/events")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, RelativeURLs) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/relative_urls")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Query) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/query")) << message_;
}

// TODO(crbug.com/40254426): Move to tabs_interactive_test.cc
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40890826): Re-enable once flakiness is fixed.
IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, DISABLED_Highlight) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/highlight")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, LastAccessed) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/last_accessed")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, CrashBrowser) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/crash")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Opener) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/opener")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Remove) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/remove")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, RemoveMultiple) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/remove_multiple")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, GetCurrent) {
  ASSERT_TRUE(RunExtensionTest("tabs/get_current")) << message_;
}

// Disabled for being flaky. See crbug.com/1472144
IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, DISABLED_Connect) {
  ASSERT_TRUE(RunExtensionTest("tabs/connect")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, OnRemoved) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_removed")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, Reload) {
  ASSERT_TRUE(RunExtensionTest("tabs/reload")) << message_;
}

class ExtensionApiCaptureTest
    : public ExtensionApiTabTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionApiCaptureTest() : ExtensionApiTabTest(GetParam()) {}
  ~ExtensionApiCaptureTest() override = default;
  ExtensionApiCaptureTest(const ExtensionApiCaptureTest&) = delete;
  ExtensionApiCaptureTest& operator=(const ExtensionApiCaptureTest&) = delete;

  void SetUp() override {
    extensions::TabsCaptureVisibleTabFunction::set_disable_throttling_for_tests(
        true);
    EnablePixelOutput();
    ExtensionApiTabTest::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionApiCaptureTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionApiCaptureTest,
                         ::testing::Values(ContextType::kServiceWorker));

// https://crbug.com/1450747 Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CaptureVisibleTabJpeg DISABLED_CaptureVisibleTabJpeg
#else
#define MAYBE_CaptureVisibleTabJpeg CaptureVisibleTabJpeg
#endif
IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, MAYBE_CaptureVisibleTabJpeg) {
  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab/test_jpeg"))
      << message_;
}

// https://crbug.com/1450933 Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CaptureVisibleTabPng DISABLED_CaptureVisibleTabPng
#else
#define MAYBE_CaptureVisibleTabPng CaptureVisibleTabPng
#endif
IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, MAYBE_CaptureVisibleTabPng) {
  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab/test_png"))
      << message_;
}

// TODO(crbug.com/40168659) Re-enable test
IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest,
                       DISABLED_CaptureVisibleTabRace) {
  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab/test_race"))
      << message_;
}

// https://crbug.com/1107934 Flaky on Windows, Linux, ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CaptureVisibleFile DISABLED_CaptureVisibleFile
#else
#define MAYBE_CaptureVisibleFile CaptureVisibleFile
#endif
IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, MAYBE_CaptureVisibleFile) {
  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab/test_file", {},
                               {.allow_file_access = true}))
      << message_;
}

// TODO(crbug.com/40803947): Fix flakiness on Linux and Lacros then reenable.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_CaptureVisibleDisabled DISABLED_CaptureVisibleDisabled
#else
#define MAYBE_CaptureVisibleDisabled CaptureVisibleDisabled
#endif
IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, MAYBE_CaptureVisibleDisabled) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots,
                                               true);
  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab/test_disabled"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiCaptureTest, CaptureNullWindow) {
  ASSERT_TRUE(RunExtensionTest("tabs/capture_visible_tab_null_window"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, OnCreated) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_created")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType,
                       LazyBackgroundTabsOnCreated) {
  ASSERT_TRUE(RunExtensionTest("tabs/lazy_background_on_created")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, OnUpdated) {
  ASSERT_TRUE(RunExtensionTest("tabs/on_updated")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabBackForwardCacheTest, OnUpdated) {
  ASSERT_TRUE(RunExtensionTest("tabs/backForwardCache/on_updated")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, NoPermissions) {
  ASSERT_TRUE(RunExtensionTest("tabs/no_permissions")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType,
                       DISABLED_HostPermission) {
  ASSERT_TRUE(RunExtensionTest("tabs/host_permission")) << message_;
}

// Flaky on Windows, Mac and Linux. http://crbug.com/820110.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#define MAYBE_UpdateWindowResize DISABLED_UpdateWindowResize
#else
#define MAYBE_UpdateWindowResize UpdateWindowResize
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_UpdateWindowResize) {
  ASSERT_TRUE(RunExtensionTest("window_update/resize")) << message_;
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, FocusWindowDoesNotUnmaximize) {
  HWND window =
      browser()->window()->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  ::SendMessage(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
  ASSERT_TRUE(RunExtensionTest("window_update/focus")) << message_;
  ASSERT_TRUE(::IsZoomed(window));
}
#endif  // BUILDFLAG(IS_WIN)

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
// Maximizing/fullscreen popup window doesn't work on aura's managed mode.
// See bug crbug.com/116305.
// Mac: http://crbug.com/103912
#define MAYBE_UpdateWindowShowState DISABLED_UpdateWindowShowState
#else
#define MAYBE_UpdateWindowShowState UpdateWindowShowState
#endif  // defined(USE_AURA) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, MAYBE_UpdateWindowShowState) {
  ASSERT_TRUE(RunExtensionTest("window_update/show_state")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType,
                       IncognitoDisabledByPref) {
  IncognitoModePrefs::SetAvailability(
      browser()->profile()->GetPrefs(),
      policy::IncognitoModeAvailability::kDisabled);

  // This makes sure that creating an incognito window fails due to pref
  // (policy) being set.
  ASSERT_TRUE(RunExtensionTest("tabs/incognito_disabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, GetViewsOfCreatedPopup) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics",
                               {.extension_url = "get_views_popup.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, GetViewsOfCreatedWindow) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics",
                               {.extension_url = "get_views_window.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType,
                       OnUpdatedDiscardedState) {
  ASSERT_TRUE(RunExtensionTest("tabs/basics/discarded")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType, OpenerCraziness) {
  ASSERT_TRUE(RunExtensionTest("tabs/tab_opener_id")) << message_;
}

// Tests sending messages from an extension's service worker using
// chrome.tabs.sendMessage to a webpage in the extension listening for them
// using chrome.runtime.OnMessage.
IN_PROC_BROWSER_TEST_F(ExtensionApiTabTest, SendMessage) {
  ASSERT_TRUE(RunExtensionTest("tabs/send_message"));
}

// Tests that extension with "tabs" permission does not leak tab info to another
// extension without "tabs" permission.
//
// Regression test for https://crbug.com/1302959
IN_PROC_BROWSER_TEST_P(ExtensionApiTabTestWithContextType,
                       TabsPermissionDoesNotLeakTabInfo) {
  constexpr char kManifestWithTabsPermission[] =
      R"({
        "name": "test", "version": "1", "manifest_version": 2,
        "background": {"scripts": ["background.js"], "persistent": true},
        "permissions": ["tabs"]
      })";
  constexpr char kBackgroundJSWithTabsPermission[] =
      "chrome.tabs.onUpdated.addListener(() => {});";

  constexpr char kManifestWithoutTabsPermission[] =
      R"({
        "name": "test", "version": "1", "manifest_version": 2,
        "background": {"scripts": ["background.js"], "persistent": true}
      })";
  constexpr char kBackgroundJSWithoutTabsPermission[] =
      R"(
        let urlStr = '%s';
        chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
          chrome.test.assertEq(3, Array.from(arguments).length);
          // Note: we'll search within all of the arguments, just to make sure
          // we don't miss any inadvertently added ones. See
          // https://crbug.com/1302959 for details.
          let argumentsStr = JSON.stringify(arguments);
          let containsUrlStr = argumentsStr.indexOf(urlStr) != -1;
          chrome.test.assertFalse(containsUrlStr);
          if (tab.status == 'complete') {
            chrome.test.notifyPass();
          }
        });
      )";

  GURL url = embedded_test_server()->GetURL("/title1.html");

  // First load the extension with "tabs" permission.
  // Note that order is important for this regression test.
  extensions::TestExtensionDir ext_dir1;
  ext_dir1.WriteManifest(kManifestWithTabsPermission);
  ext_dir1.WriteFile(FILE_PATH_LITERAL("background.js"),
                     kBackgroundJSWithTabsPermission);
  ASSERT_TRUE(LoadExtension(ext_dir1.UnpackedPath()));

  // Then load the extension without "tabs" permission.
  extensions::ResultCatcher catcher;
  extensions::TestExtensionDir ext_dir2;
  ext_dir2.WriteManifest(kManifestWithoutTabsPermission);
  ext_dir2.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(kBackgroundJSWithoutTabsPermission,
                                        url.spec().c_str()));
  ASSERT_TRUE(LoadExtension(ext_dir2.UnpackedPath()));

  // Now open a tab and ensure the extension in |ext_dir2| does not see any info
  // that is guarded by "tabs" permission.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

struct IncognitoTestParam {
  IncognitoTestParam(bool is_incognito_enabled, ContextType context_type)
      : is_incognito_enabled(is_incognito_enabled),
        context_type(context_type) {}

  bool is_incognito_enabled;
  ContextType context_type;
};

class IncognitoExtensionApiTabTest
    : public ExtensionApiTabTest,
      public testing::WithParamInterface<IncognitoTestParam> {
 public:
  IncognitoExtensionApiTabTest()
      : ExtensionApiTabTest(GetParam().context_type) {}
  IncognitoExtensionApiTabTest(const IncognitoExtensionApiTabTest&) = delete;
  IncognitoExtensionApiTabTest& operator=(const IncognitoExtensionApiTabTest&) =
      delete;
  ~IncognitoExtensionApiTabTest() override = default;
};

IN_PROC_BROWSER_TEST_P(IncognitoExtensionApiTabTest, Tabs) {
  bool is_incognito_enabled = GetParam().is_incognito_enabled;
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  std::string args = base::StringPrintf(
      R"({"isIncognito": %s, "windowId": %d})",
      is_incognito_enabled ? "true" : "false",
      extensions::ExtensionTabUtil::GetWindowId(incognito_browser));

  EXPECT_TRUE(RunExtensionTest("tabs/basics/incognito",
                               {.custom_arg = args.c_str()},
                               {.allow_in_incognito = is_incognito_enabled}))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    PB_IncognitoEnabled,
    IncognitoExtensionApiTabTest,
    testing::Values(IncognitoTestParam(true,
                                       ContextType::kPersistentBackground)));
INSTANTIATE_TEST_SUITE_P(
    PB_IncognitoDisabled,
    IncognitoExtensionApiTabTest,
    testing::Values(IncognitoTestParam(false,
                                       ContextType::kPersistentBackground)));
INSTANTIATE_TEST_SUITE_P(
    SW_IncognitoEnabled,
    IncognitoExtensionApiTabTest,
    testing::Values(IncognitoTestParam(true, ContextType::kServiceWorker)));
INSTANTIATE_TEST_SUITE_P(
    SW_IncognitoDisabled,
    IncognitoExtensionApiTabTest,
    testing::Values(IncognitoTestParam(false, ContextType::kServiceWorker)));

class ExtensionApiTabPrerenderingTest : public ExtensionApiTabTest {
 public:
  ExtensionApiTabPrerenderingTest()
      : prerender_helper_(base::BindRepeating(
            &ExtensionApiTabPrerenderingTest::GetWebContents,
            base::Unretained(this))) {}
  ~ExtensionApiTabPrerenderingTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// TODO(crbug.com/40235049): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(ExtensionApiTabPrerenderingTest, DISABLED_Prerendering) {
  ASSERT_TRUE(RunExtensionTest("tabs/prerendering")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTabPrerenderingTest,
                       PrerenderingIntoANewTab) {
  ASSERT_TRUE(RunExtensionTest("tabs/prerendering_into_new_tab")) << message_;
}

// Adding a new test? Awesome. But API tests are the old hotness. The new
// hotness is api_test_utils. See tabs_test.cc for an example.
// We are trying to phase out many uses of API tests as they tend to be flaky.
