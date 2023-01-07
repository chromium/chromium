// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/build_info.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace policy {

// The test changes per-profile pref that affects a WebSetting. Blink code uses
// the said web setting to gate access to immersive-ar sessions. The test will
// check whether the setting is propagated correctly to blink.
class PolicyTestWebXRImmersiveAR : public AndroidBrowserTest {
 public:
  void SetUp() override {
    if (ShouldSkipTest()) {
      GTEST_SKIP();
    }

    AndroidBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // immersive-ar sessions are only enabled on Android N and higher - when run
  // on older OSes, we will not be able to distinguish whether the
  // isSessionSupported() call returned `false` because of the preference, or
  // because the AR provider was not available due to the runtime OS check.
  // We should skip the test if we know that this is what will happen.
  bool ShouldSkipTest() {
    return base::android::BuildInfo::GetInstance()->sdk_int() <
           base::android::SDK_VERSION_NOUGAT;
  }
};

IN_PROC_BROWSER_TEST_F(PolicyTestWebXRImmersiveAR,
                       CheckImmersiveARWorksWhenEnabled) {
  // Simulate enabling enterprise policy:
  Profile* profile = chrome_test_utils::GetProfile(this);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kWebXRImmersiveArEnabled, true);

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);

  // Refresh web preferences on web contents:
  web_contents->OnWebPreferencesChanged();

  // Navigate somewhere - doesn't matter where, as long as we'll be in a
  // secure context (otherwise, navigator.xr is inaccessible):
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));

  EXPECT_EQ(true, content::EvalJs(web_contents, R"(
      navigator.xr.isSessionSupported("immersive-ar")
    )"));
}

IN_PROC_BROWSER_TEST_F(PolicyTestWebXRImmersiveAR,
                       CheckImmersiveARWorksWhenNotSet) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  PrefService* prefs = profile->GetPrefs();

  // By default, the policy-controlled pref should be enabled:
  EXPECT_TRUE(prefs->GetBoolean(prefs::kWebXRImmersiveArEnabled));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);

  // Refresh web preferences on web contents:
  web_contents->OnWebPreferencesChanged();

  // Navigate somewhere - doesn't matter where, as long as we'll be in a
  // secure context (otherwise, navigator.xr is inaccessible):
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));

  EXPECT_EQ(true, content::EvalJs(web_contents, R"(
      navigator.xr.isSessionSupported("immersive-ar")
    )"));
}

IN_PROC_BROWSER_TEST_F(PolicyTestWebXRImmersiveAR,
                       CheckImmersiveARFailsWhenDisabled) {
  // Simulate disabling enterprise policy:
  Profile* profile = chrome_test_utils::GetProfile(this);
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kWebXRImmersiveArEnabled, false);

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);

  // Refresh web preferences on web contents:
  web_contents->OnWebPreferencesChanged();

  // Navigate somewhere - doesn't matter where, as long as we'll be in a
  // secure context (otherwise, navigator.xr is inaccessible):
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));

  EXPECT_EQ(false, content::EvalJs(web_contents, R"(
      navigator.xr.isSessionSupported("immersive-ar")
    )"));
}

}  // namespace policy
