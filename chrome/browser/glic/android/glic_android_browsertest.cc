// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/device_info.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class GlicAndroidMojoBrowserTest : public GlicApiBrowserTest {
 public:
  GlicAndroidMojoBrowserTest()
      : GlicApiBrowserTest("./glic_android_browsertest.js") {}
};

IN_PROC_BROWSER_TEST_F(GlicAndroidMojoBrowserTest, testAllTestsAreRegistered) {
  ASSERT_OK(OpenGlicForActiveTab());
  AssertAllTestsRegistered({"GlicAndroidMojoBrowserTest"});
}

// Tests that page context fetching can query the URL of the focused browser
// tab.
IN_PROC_BROWSER_TEST_F(GlicAndroidMojoBrowserTest, testPageContextFetching) {
  // 1. Setup mock page to query context from
  GURL page_url = GetTestUrl("page.html");
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(), page_url));

  // 2. Open Glic and wait for client to bind
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(WaitForGlicClient(instance).has_value());

  // Enable default tab context sharing
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       true);

  // 3. Trigger TypeScript test method via GlicApiBrowserTest helper
  ExecuteJsTest();
}

// Tests that Mojo pipe stays resilient when device screen orientation changes.
IN_PROC_BROWSER_TEST_F(GlicAndroidMojoBrowserTest,
                       testDeviceRotationMojoResiliency) {
  // TODO (crbug.com/545025371): The test is consistenty failing on automotive
  // bot. Disabled by the gardener.
  if (base::android::device_info::is_desktop() ||
      base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "Screen rotation is not applicable on desktop Android.";
  }

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  ASSERT_TRUE(content::NavigateToURL(tab_contents, GetTestUrl("page.html")));

  // 1. Open Glic and wait for client connection
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(WaitForGlicClient(instance).has_value());

  // Execute TypeScript test helper to confirm connection is ready.
  ExecuteJsTest();

  // 2. Simulate device rotation to landscape (SCREEN_ORIENTATION_LANDSCAPE = 0)
  SetActivityOrientationForTesting(tab_contents, 0);

  // 3. Ensure Glic client Mojo connection remains bound after rotation
  EXPECT_TRUE(RunUntil(
      [this]() { return GetOnlyGlicInstance()->host().IsWebClientConnected(); },
      "Mojo connection disconnected post-rotation"));

  // 4. Restore original portrait orientation (SCREEN_ORIENTATION_PORTRAIT = 1)
  SetActivityOrientationForTesting(tab_contents, 1);

  EXPECT_TRUE(RunUntil(
      [this]() { return GetOnlyGlicInstance()->host().IsWebClientConnected(); },
      "Mojo connection disconnected post-rotation restore"));
}

}  // namespace

}  // namespace glic
