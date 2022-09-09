// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>

#include "ash/shell.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/test/display_manager_test_api.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

bool RunGetDisplayMediaSet(content::WebContents* tab,
                           const std::string& constraints) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetPrimaryMainFrame(),
      base::StringPrintf("runGetDisplayMediaSet(%s);", constraints.c_str()),
      &result));

  return result == "capture-success";
}

size_t GetStreamCount(content::WebContents* tab) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetPrimaryMainFrame(), "getStreamCount()", &result));
  size_t stream_count = 0;
  EXPECT_TRUE(base::StringToSizeT(result, &stream_count));
  return stream_count;
}

class ContentBrowserClientMock : public ChromeContentBrowserClient {
 public:
  bool IsGetDisplayMediaSetSelectAllScreensAllowed(
      content::BrowserContext* context,
      const url::Origin& origin) override {
    return true;
  }
};

}  // namespace

class GetDisplayMediaSetBrowserTest : public WebRtcTestBase {
 public:
  GetDisplayMediaSetBrowserTest() {
    scoped_feature_list_.InitFromCommandLine(
        /*enable_features=*/
        "GetDisplayMediaSet,GetDisplayMediaSetAutoSelectAllScreens",
        /*disable_features=*/"");
  }

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();
    browser_client_ = std::make_unique<ContentBrowserClientMock>();
    content::SetBrowserClientForTesting(browser_client_.get());
    ASSERT_TRUE(embedded_test_server()->Start());
    contents_ =
        OpenTestPageInNewTab("/webrtc/webrtc_getdisplaymediaset_test.html");
    DCHECK(contents_);
  }

  void SetScreens(size_t screen_count) {
    // This part of the test only works on ChromeOS.
    std::stringstream screens;
    for (size_t screen_index = 0; screen_index + 1 < screen_count;
         screen_index++) {
      // Each entry in this comma separated list corresponds to a screen
      // specification following the format defined in
      // |ManagedDisplayInfo::CreateFromSpec|.
      // The used specficiation simulatoes screens with resolution 800x800
      // at the host coordinates (screen_index * 800, 0).
      screens << screen_index * 640 << "+0-640x480,";
    }
    if (screen_count != 0) {
      screens << (screen_count - 1) * 640 << "+0-640x480";
    }
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay(screens.str());
  }

 protected:
  content::WebContents* contents_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ContentBrowserClientMock> browser_client_;
  std::vector<aura::Window*> windows_;
};

IN_PROC_BROWSER_TEST_F(GetDisplayMediaSetBrowserTest,
                       GetDisplayMediaSetSingleScreenSuccess) {
  SetScreens(/*screen_count=*/1u);
  EXPECT_TRUE(RunGetDisplayMediaSet(contents_, "{autoSelectAllScreens: true}"));
  EXPECT_EQ(1u, GetStreamCount(contents_));
}

IN_PROC_BROWSER_TEST_F(GetDisplayMediaSetBrowserTest,
                       GetDisplayMediaSetNoScreenSuccess) {
  SetScreens(/*screen_count=*/0u);
  EXPECT_TRUE(RunGetDisplayMediaSet(contents_, "{autoSelectAllScreens: true}"));
  // If no screen is attached to a device, the |DisplayManager| will add a
  // default device. This same behavior is used in other places in Chrome that
  // handle multiple screens (e.g. in the window placement API) and
  // getDisplayMediaSet will follow the same convention.
  EXPECT_EQ(1u, GetStreamCount(contents_));
}

IN_PROC_BROWSER_TEST_F(GetDisplayMediaSetBrowserTest,
                       GetDisplayMediaSetMultipleScreensSuccess) {
  SetScreens(/*screen_count=*/5u);
  EXPECT_TRUE(RunGetDisplayMediaSet(contents_, "{autoSelectAllScreens: true}"));
  EXPECT_EQ(5u, GetStreamCount(contents_));
}

#endif
