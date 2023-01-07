// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ash/shell.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
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
                           const std::string& constraints,
                           std::vector<std::string>& track_ids) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetPrimaryMainFrame(),
      base::StringPrintf("runGetDisplayMediaSet(%s);", constraints.c_str()),
      &result));
  if (result == "capture-failure")
    return false;
  track_ids = base::SplitString(result, ",", base::KEEP_WHITESPACE,
                                base::SPLIT_WANT_ALL);
  return true;
}

bool CheckScreenDetailedExists(content::WebContents* tab,
                               const std::string& track_id) {
  std::string result;
  const char* video_track_contains_screen_details_call =
      R"JS(videoTrackContainsScreenDetailed("%s"))JS";
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab->GetPrimaryMainFrame(),
      base::StringPrintf(video_track_contains_screen_details_call,
                         track_id.c_str()),
      &result));
  return result == "success-screen-detailed";
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
  std::vector<std::string> track_ids;
  EXPECT_TRUE(RunGetDisplayMediaSet(contents_, "{autoSelectAllScreens: true}",
                                    track_ids));
  EXPECT_EQ(1u, track_ids.size());
}

IN_PROC_BROWSER_TEST_F(GetDisplayMediaSetBrowserTest,
                       GetDisplayMediaSetNoScreenSuccess) {
  SetScreens(/*screen_count=*/0u);
  std::vector<std::string> track_ids;
  EXPECT_TRUE(RunGetDisplayMediaSet(contents_, "{autoSelectAllScreens: true}",
                                    track_ids));
  // If no screen is attached to a device, the |DisplayManager| will add a
  // default device. This same behavior is used in other places in Chrome that
  // handle multiple screens (e.g. in the window placement API) and
  // getDisplayMediaSet will follow the same convention.
  EXPECT_EQ(1u, track_ids.size());
  EXPECT_EQ(track_ids.size(), base::flat_set<std::string>(track_ids).size());
}

IN_PROC_BROWSER_TEST_F(GetDisplayMediaSetBrowserTest,
                       GetDisplayMediaSetMultipleScreensSuccess) {
  SetScreens(/*screen_count=*/5u);
  std::vector<std::string> track_ids;
  EXPECT_TRUE(RunGetDisplayMediaSet(contents_, "{autoSelectAllScreens: true}",
                                    track_ids));
  EXPECT_EQ(5u, track_ids.size());
}

IN_PROC_BROWSER_TEST_F(GetDisplayMediaSetBrowserTest,
                       TrackContainsScreenDetailed) {
  SetScreens(/*screen_count=*/1u);
  std::vector<std::string> track_ids;
  EXPECT_TRUE(RunGetDisplayMediaSet(contents_, "{autoSelectAllScreens: true}",
                                    track_ids));
  ASSERT_EQ(1u, track_ids.size());

  EXPECT_TRUE(CheckScreenDetailedExists(contents_, track_ids[0]));
}

IN_PROC_BROWSER_TEST_F(GetDisplayMediaSetBrowserTest,
                       MultipleTracksContainScreenDetailed) {
  SetScreens(/*screen_count=*/5u);
  std::vector<std::string> track_ids;
  EXPECT_TRUE(RunGetDisplayMediaSet(contents_, "{autoSelectAllScreens: true}",
                                    track_ids));
  EXPECT_EQ(5u, track_ids.size());
  EXPECT_EQ(track_ids.size(), base::flat_set<std::string>(track_ids).size());

  for (const std::string& track_id : track_ids) {
    std::string screen_detailed_attributes;
    EXPECT_TRUE(CheckScreenDetailedExists(contents_, track_id));
  }
}

#endif
