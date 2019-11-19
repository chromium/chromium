// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"

namespace {

class ChromeDoNotTrackTest : public InProcessBrowserTest {
 protected:
  void SetEnableDoNotTrack(bool enabled) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kEnableDoNotTrack, enabled);
  }

  void ExpectPageTextEq(const std::string& expected_content) {
    std::string text;
    ASSERT_TRUE(ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.body.innerText);",
        &text));
    EXPECT_EQ(expected_content, text);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, NotEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(false /* enabled */);

  GURL url = embedded_test_server()->GetURL("/echoheader?DNT");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(false,
            GetWebContents()->GetMutableRendererPrefs()->enable_do_not_track);
  ExpectPageTextEq("None");
}

IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, Enabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  GURL url = embedded_test_server()->GetURL("/echoheader?DNT");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(true,
            GetWebContents()->GetMutableRendererPrefs()->enable_do_not_track);
  ExpectPageTextEq("1");
}

// Checks that the DNT header is preserved when fetching from a dedicated
// worker.
IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, FetchFromWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/workers/fetch_from_worker.html?script=fetch_from_worker.js"));
  EXPECT_EQ("1",
            EvalJs(GetWebContents(), "fetch_from_worker('/echoheader?DNT');"));

  // Updating settings should be reflected immediately.
  SetEnableDoNotTrack(false /* enabled */);
  EXPECT_EQ("None",
            EvalJs(GetWebContents(), "fetch_from_worker('/echoheader?DNT');"));
}

// Checks that the DNT header is preserved when fetching from a dedicated
// worker created from a dedicated worker.
IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, FetchFromNestedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/workers/fetch_from_worker.html?"
                                     "script=fetch_from_nested_worker.js"));
  EXPECT_EQ("1",
            EvalJs(GetWebContents(), "fetch_from_worker('/echoheader?DNT');"));

  // Updating settings should be reflected immediately.
  SetEnableDoNotTrack(false /* enabled */);
  EXPECT_EQ("None",
            EvalJs(GetWebContents(), "fetch_from_worker('/echoheader?DNT');"));
}

// Checks that the DNT header is preserved when fetching from a shared worker.
//
// Disabled on Android since a shared worker is not available on Android:
// crbug.com/869745.
#if defined(OS_ANDROID)
#define MAYBE_FetchFromSharedWorker DISABLED_FetchFromSharedWorker
#else
#define MAYBE_FetchFromSharedWorker FetchFromSharedWorker
#endif
IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, MAYBE_FetchFromSharedWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/workers/fetch_from_shared_worker.html"));
  EXPECT_EQ("1", EvalJs(GetWebContents(),
                        "fetch_from_shared_worker('/echoheader?DNT');"));

  // Updating settings should be reflected immediately.
  SetEnableDoNotTrack(false /* enabled */);
  EXPECT_EQ("None", EvalJs(GetWebContents(),
                           "fetch_from_shared_worker('/echoheader?DNT');"));
}

// Checks that the DNT header is preserved when fetching from a service worker.
IN_PROC_BROWSER_TEST_F(ChromeDoNotTrackTest, FetchFromServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetEnableDoNotTrack(true /* enabled */);

  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(
                                   "/workers/fetch_from_service_worker.html"));
  EXPECT_EQ("ready", EvalJs(GetWebContents(), "setup();"));
  EXPECT_EQ("1", EvalJs(GetWebContents(),
                        "fetch_from_service_worker('/echoheader?DNT');"));

  // Updating settings should be reflected immediately.
  SetEnableDoNotTrack(false /* enabled */);
  EXPECT_EQ("None", EvalJs(GetWebContents(),
                           "fetch_from_service_worker('/echoheader?DNT');"));
}

}  // namespace
