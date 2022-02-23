// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

// TODO(crbug.com/1215089): Enable this test suite on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using content::WebContents;

// TODO(crbug.com/1247761): Add tests that verify excessive calls to
// produceCropId() yield the empty string.
MATCHER(IsEmptyCropId, "") {
  static_assert(std::is_same<decltype(arg), const std::string&>::value, "");
  return arg == "empty-crop-id";  // See region_capture_main.html for rationale.
}

MATCHER(IsValidCropId, "") {
  static_assert(std::is_same<decltype(arg), const std::string&>::value, "");
  return base::GUID::ParseLowercase(arg).is_valid();
}

const char kMainPageTitle[] = "Region Capture Test - Page 1 (Main)";
const char kOtherPageTitle[] = "Region Capture Test - Page 2 (Main)";

// Tracks CropIdWebContentsHelper::kMaxCropIdsPerWebContents.
constexpr size_t kMaxCropIdsPerWebContents = 100;

enum {
  kMainPageTopLevelDocument,
  kMainPageEmbeddedDocument,
  kOtherPageTopLevelDocument,
  kOtherPageEmbeddedDocument,
  kServerCount  // Must be last.
};

enum {
  kMainTab,
  kOtherTab,
  kTabCount  // Must be last.
};

enum class Frame {
  kNone,
  kTopLevelDocument,
  kEmbeddedFrame,
};

// Conveniently pack together all relevant information about a tab and
// conveniently expose test controls on it.
struct TabInfo {
  void StartEmbeddingFrame(const GURL& url) {
    std::string script_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(),
        base::StringPrintf("startEmbeddingFrame('%s');", url.spec().c_str()),
        &script_result));
    EXPECT_EQ(script_result, "embedding-done");
  }

  void StartCapture() {
    // Bring the tab into focus. This avoids getDisplayMedia rejection.
    browser->tab_strip_model()->ActivateTabAt(tab_strip_index);

    std::string script_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(), "startCapture();", &script_result));
    EXPECT_EQ(script_result, "capture-success");
  }

  void StartCaptureFromEmbeddedFrame() {
    std::string script_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(), "startCaptureFromEmbeddedFrame();",
        &script_result));
    EXPECT_EQ(script_result, "embedded-capture-success");
  }

  std::string ProduceCropId(Frame frame,
                            const std::string& element_id = "div") {
    DCHECK_NE(frame, Frame::kNone);
    const std::string frame_js =
        (frame == Frame::kTopLevelDocument) ? "top" : "embedded";
    std::string script_result = "error-not-modified";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(),
        base::StrCat(
            {"produceCropId(\"", frame_js, "\", \"" + element_id + "\");"}),
        &script_result));
    return script_result;
  }

  std::string CropTo(const std::string& crop_id) {
    std::string script_result = "error-not-modified";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(),
        base::StrCat({"cropTo(\"", crop_id, "\");"}), &script_result));
    return script_result;
  }

  std::string CreateNewDivElement(Frame frame, const std::string& div_id) {
    DCHECK_NE(frame, Frame::kNone);
    const std::string frame_js =
        (frame == Frame::kTopLevelDocument) ? "top" : "embedded";
    std::string script_result = "error-not-modified";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(),
        base::StrCat(
            {"createNewDivElement(\"", frame_js, "\", \"", div_id, "\");"}),
        &script_result));
    return script_result;
  }

  Browser* browser;
  WebContents* web_contents;
  int tab_strip_index;
};

}  // namespace

// Essentially depends on InProcessBrowserTest, but WebRtcTestBase provides
// detection of JS errors.
class RegionCaptureBrowserTest : public WebRtcTestBase {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    WebRtcTestBase::SetUpInProcessBrowserTestFixture();

    DetectErrorsInJavaScript();

    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));

    for (int i = 0; i < kServerCount; ++i) {
      servers_.emplace_back(std::make_unique<net::EmbeddedTestServer>());
      servers_[i]->ServeFilesFromDirectory(test_dir);
      ASSERT_TRUE(servers_[i]->Start());
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line_ = command_line;
  }

  void TearDownOnMainThread() override {
    for (auto& server : servers_) {
      if (server) {
        ASSERT_TRUE(server->ShutdownAndWaitUntilComplete());
      }
    }

    WebRtcTestBase::TearDownOnMainThread();
  }

  // Same as WebRtcTestBase::OpenTestPageInNewTab, but does not assume
  // a single embedded server is used for all pages, and also embeds
  // a cross-origin iframe.
  void SetUpPage(const std::string& top_level_document_document,
                 net::EmbeddedTestServer* top_level_document_server,
                 const std::string& embedded_iframe_document,
                 net::EmbeddedTestServer* embedded_iframe_server,
                 TabInfo* tab_info) const {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        top_level_document_server->GetURL(top_level_document_document)));

    WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    // web_contents_vector_.push_back(web_contents);
    permissions::PermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(
            permissions::PermissionRequestManager::ACCEPT_ALL);

    *tab_info = {browser(), web_contents,
                 browser()->tab_strip_model()->active_index()};
    tab_info->StartEmbeddingFrame(
        embedded_iframe_server->GetURL(embedded_iframe_document));
  }

  // Set up all (necessary) tabs, loads iframes, and start capturing the
  // relevant tab.
  void SetUpTest(Frame capturing_entity, bool self_capture) {
    // Main page (for self-capture).
    SetUpPage("/webrtc/region_capture_main.html",
              servers_[kMainPageTopLevelDocument].get(),
              "/webrtc/region_capture_embedded.html",
              servers_[kMainPageEmbeddedDocument].get(), &tabs_[kMainTab]);

    if (!self_capture) {
      // Other page (for other-tab-capture).
      SetUpPage("/webrtc/region_capture_other_main.html",
                servers_[kOtherPageTopLevelDocument].get(),
                "/webrtc/region_capture_other_embedded.html",
                servers_[kOtherPageEmbeddedDocument].get(), &tabs_[kOtherTab]);
    }

    DCHECK(command_line_);
    command_line_->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle,
        self_capture ? kMainPageTitle : kOtherPageTitle);

    if (capturing_entity == Frame::kTopLevelDocument) {
      tabs_[kMainTab].StartCapture();
    } else if (capturing_entity == Frame::kEmbeddedFrame) {
      tabs_[kMainTab].StartCaptureFromEmbeddedFrame();
    }
  }

  // Manipulation after SetUpCommandLine, but before capture starts,
  // allows tests to set which tab to capture.
  base::CommandLine* command_line_ = nullptr;

  // Holds the tabs manipulated by this test.
  TabInfo tabs_[kTabCount];

  // Each page is served from a distinct origin, thereby proving that cropping
  // works irrespective of whether iframes are in/out-of-process.
  std::vector<std::unique_ptr<net::EmbeddedTestServer>> servers_;
};

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       ProduceCropIdReturnsValidIdInMainPage) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  EXPECT_THAT(tabs_[kMainTab].ProduceCropId(Frame::kTopLevelDocument),
              IsValidCropId());
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       ProduceCropIdReturnsValidIdInCrossOriginIframe) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  EXPECT_THAT(tabs_[kMainTab].ProduceCropId(Frame::kEmbeddedFrame),
              IsValidCropId());
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       ProduceCropIdReturnsSameIdIfSameElement) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  EXPECT_EQ(tabs_[kMainTab].ProduceCropId(Frame::kTopLevelDocument),
            tabs_[kMainTab].ProduceCropId(Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToAllowedIfTopLevelCropsToElementInTopLevel) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_id, IsValidCropId());
  EXPECT_EQ(tab.CropTo(crop_id), "top-level-crop-success");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToAllowedIfTopLevelCropsToElementInEmbedded) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kEmbeddedFrame);
  ASSERT_THAT(crop_id, IsValidCropId());
  EXPECT_EQ(tab.CropTo(crop_id), "top-level-crop-success");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToAllowedIfEmbeddedFrameCropsToElementInTopLevel) {
  SetUpTest(Frame::kEmbeddedFrame, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_id, IsValidCropId());
  EXPECT_EQ(tab.CropTo(crop_id), "embedded-crop-success");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToAllowedIfEmbeddedFrameCropsToElementInEmbedded) {
  SetUpTest(Frame::kEmbeddedFrame, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kEmbeddedFrame);
  ASSERT_THAT(crop_id, IsValidCropId());
  EXPECT_EQ(tab.CropTo(crop_id), "embedded-crop-success");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest, CropToAllowedToUncrop) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_id, IsValidCropId());
  ASSERT_EQ(tab.CropTo(crop_id), "top-level-crop-success");

  EXPECT_EQ(tab.CropTo(""), "top-level-crop-success");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest, CropToRejectedIfUnknown) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_id, IsValidCropId());

  DCHECK(!crop_id.empty());  // Test sanity.
  const std::string::value_type wrong_char = (crop_id[0] == 'a' ? 'b' : 'a');
  const std::string wrong_crop_id = wrong_char + crop_id.substr(1);
  EXPECT_EQ(tab.CropTo(wrong_crop_id), "top-level-crop-error");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest, CropToRejectedIfInvalid) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_id, IsValidCropId());

  EXPECT_EQ(tab.CropTo("invalid-crop-id"), "top-level-crop-error");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToRejectedIfProduceCropIdWasNeverCalled) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_id, IsValidCropId());

  const std::string kValidCropId = "01234567-0123-0123-0123-0123456789ab";
  ASSERT_THAT(kValidCropId, IsValidCropId());  // Test is sane.

  EXPECT_EQ(tab.CropTo(kValidCropId), "top-level-crop-error");
}

IN_PROC_BROWSER_TEST_F(
    RegionCaptureBrowserTest,
    CropToForUncroppingRejectedIfProduceCropIdWasCalledButTrackUncropped) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_id = tab.ProduceCropId(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_id, IsValidCropId());
  // CropTo(crop_id) with a non-empty |crop_id| is intentionally not called.
  // Instead, the test immediately calls CropTo("") on a still-uncropped track,
  // attempting to stop cropping when no cropping was ever specified.
  EXPECT_EQ(tab.CropTo(""), "top-level-crop-success");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToRejectedIfElementInAnotherTabTopLevel) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/false);

  const std::string crop_id =
      tabs_[kOtherTab].ProduceCropId(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_id, IsValidCropId());

  EXPECT_EQ(tabs_[kMainTab].CropTo(crop_id), "top-level-crop-error");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToRejectedIfElementInAnotherTabEmbeddedFrame) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/false);

  const std::string crop_id =
      tabs_[kOtherTab].ProduceCropId(Frame::kEmbeddedFrame);
  ASSERT_THAT(crop_id, IsValidCropId());

  EXPECT_EQ(tabs_[kMainTab].CropTo(crop_id), "top-level-crop-error");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest, MaxCropIdsInTopLevelDocument) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  // Create kMaxCropIdsPerWebContents new elements and assign each a crop-ID.
  std::set<std::string> crop_ids;
  for (size_t i = 0; i < kMaxCropIdsPerWebContents; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_EQ(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id),
              "top-level-new-div-success");
    const std::string crop_id =
        tab.ProduceCropId(Frame::kTopLevelDocument, element_id);
    ASSERT_THAT(crop_id, IsValidCropId());
    crop_ids.insert(crop_id);
  }
  EXPECT_EQ(crop_ids.size(), kMaxCropIdsPerWebContents);

  // Create one more element - this one won't get a crop-ID.
  const std::string element_id =
      ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents));
  ASSERT_EQ(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id),
            "top-level-new-div-success");
  EXPECT_EQ(tab.ProduceCropId(Frame::kTopLevelDocument, element_id),
            "top-level-produce-crop-id-error");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest, MaxCropIdsInEmbeddedFrame) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  // Create kMaxCropIdsPerWebContents new elements and assign each a crop-ID.
  std::set<std::string> crop_ids;
  for (size_t i = 0; i < kMaxCropIdsPerWebContents; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_EQ(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id),
              "embedded-new-div-success");
    const std::string crop_id =
        tab.ProduceCropId(Frame::kEmbeddedFrame, element_id);
    ASSERT_THAT(crop_id, IsValidCropId());
    crop_ids.insert(crop_id);
  }
  EXPECT_EQ(crop_ids.size(), kMaxCropIdsPerWebContents);

  // Create one more element - this one won't get a crop-ID.
  const std::string element_id =
      ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents));
  ASSERT_EQ(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id),
            "embedded-new-div-success");
  EXPECT_EQ(tab.ProduceCropId(Frame::kEmbeddedFrame, element_id),
            "embedded-produce-crop-id-error");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       MaxCropIdsSharedBetweenFramesInTab) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  static_assert(kMaxCropIdsPerWebContents > 1, "");

  // Create (kMaxCropIdsPerWebContents - 1) new elements and assign each a
  // crop-ID.
  std::set<std::string> crop_ids;
  for (size_t i = 0; i < kMaxCropIdsPerWebContents - 1; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_EQ(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id),
              "top-level-new-div-success");
    const std::string crop_id =
        tab.ProduceCropId(Frame::kTopLevelDocument, element_id);
    ASSERT_THAT(crop_id, IsValidCropId());
    crop_ids.insert(crop_id);
  }
  EXPECT_EQ(crop_ids.size(), kMaxCropIdsPerWebContents - 1);

  // One more in the embedded frame is possible.
  std::string element_id =
      ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents - 1));
  ASSERT_EQ(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id),
            "embedded-new-div-success");
  std::string crop_id = tab.ProduceCropId(Frame::kEmbeddedFrame, element_id);
  EXPECT_THAT(crop_id, IsValidCropId());
  EXPECT_TRUE(crop_ids.find(crop_id) == crop_ids.end());

  // Create one more element - this one won't get a crop-ID.
  element_id = ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents));
  ASSERT_EQ(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id),
            "top-level-new-div-success");
  EXPECT_EQ(tab.ProduceCropId(Frame::kTopLevelDocument, element_id),
            "top-level-produce-crop-id-error");
  // Neither in the top-level nor in the embedded frame.
  element_id =
      ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents + 1));
  ASSERT_EQ(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id),
            "embedded-new-div-success");
  EXPECT_EQ(tab.ProduceCropId(Frame::kEmbeddedFrame, element_id),
            "embedded-produce-crop-id-error");
}

#endif  //  !BUILDFLAG(IS_CHROMEOS_LACROS)
