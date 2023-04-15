// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
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
#include "third_party/blink/public/common/features.h"
#include "ui/gl/gl_switches.h"

// TODO(crbug.com/1215089): Enable this test suite on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using content::WebContents;
using testing::Bool;
using testing::Combine;
using testing::TestParamInfo;
using testing::Values;
using testing::WithParamInterface;

// Comparisons with CropTargets are limited. We can only know that a one was
// returned by the test. However, we keep this matcher around in anticipation
//  of CropTargets being made either comparable or stringifiable. Then we
// can do more interesting comparisons, like ensuring uniqueness or checking
// that repeatedly calling CropTarget.fromElement() on the same Element yields
// either the same CropTarget, or an equivalent one.
MATCHER_P(IsExpectedCropTarget, expected_crop_target_index, "") {
  static_assert(std::is_same<decltype(arg), const std::string&>::value, "");
  return arg == expected_crop_target_index;
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
  kMailmanServer,
  kServerCount  // Must be last.
};

enum Tab {
  kMainTab,
  kOtherTab,
  kTabCount  // Must be last.
};

enum class Frame {
  kNone,
  kTopLevelDocument,
  kEmbeddedFrame,
};

enum class Track { kOriginal, kClone, kSecond };

const char* ToString(Frame frame) {
  return frame == Frame::kNone               ? "none"
         : frame == Frame::kTopLevelDocument ? "top-level"
         : frame == Frame::kEmbeddedFrame    ? "embedded"
                                             : "error";
}

const char* ToString(Track track) {
  return track == Track::kOriginal ? "original"
         : track == Track::kClone  ? "clone"
         : track == Track::kSecond ? "second"
                                   : "error";
}

// Conveniently pack together all relevant information about a tab and
// conveniently expose test controls on it.
struct TabInfo {
  void StartEmbeddingFrame(const GURL& url) {
    EXPECT_EQ(content::EvalJs(web_contents->GetPrimaryMainFrame(),
                              base::StringPrintf("startEmbeddingFrame('%s');",
                                                 url.spec().c_str())),
              "embedding-done");
  }

  void SetUpMailman(const GURL& url) {
    EXPECT_EQ(content::EvalJs(web_contents->GetPrimaryMainFrame(),
                              base::StringPrintf("setUpMailman('%s');",
                                                 url.spec().c_str())),
              "mailman-ready");
  }

  void StartCapture() {
    // Bring the tab into focus. This avoids getDisplayMedia rejection.
    browser->tab_strip_model()->ActivateTabAt(tab_strip_index);

    EXPECT_EQ(
        content::EvalJs(web_contents->GetPrimaryMainFrame(), "startCapture();"),
        "top-level-capture-success");
  }

  void StartCaptureFromEmbeddedFrame() {
    // Bring the tab into focus. This avoids getDisplayMedia rejection.
    browser->tab_strip_model()->ActivateTabAt(tab_strip_index);

    EXPECT_EQ(content::EvalJs(web_contents->GetPrimaryMainFrame(),
                              "startCaptureFromEmbeddedFrame();"),
              "embedded-capture-success");
  }

  bool StartSecondCapture(Frame frame) {
    // Bring the tab into focus. This avoids getDisplayMedia rejection.
    browser->tab_strip_model()->ActivateTabAt(tab_strip_index);

    return content::EvalJs(
               web_contents->GetPrimaryMainFrame(),
               base::StringPrintf("startSecondCapture('%s');", ToString(frame)))
               .ExtractString() ==
           base::StrCat({ToString(frame), "-second-capture-success"});
  }

  bool StopCapture(Frame frame, Track track) {
    return content::EvalJs(web_contents->GetPrimaryMainFrame(),
                           base::StringPrintf("stopCapture('%s', '%s');",
                                              ToString(frame), ToString(track)))
               .ExtractString() ==
           base::StrCat({ToString(frame), "-stop-success"});
  }

  std::string CropTargetFromElement(Frame frame,
                                    const std::string& element_id = "div") {
    DCHECK_NE(frame, Frame::kNone);
    return content::EvalJs(
               web_contents->GetPrimaryMainFrame(),
               base::StrCat({"cropTargetFromElement(\"", ToString(frame),
                             "\", \"" + element_id + "\");"}))
        .ExtractString();
  }

  // Takes as input either the CropTarget[*], or "undefined" if the test
  // wants track.cropTo(undefined) to be invoked.
  // [*] Actually, because `CropTarget`s are not stringifiable, an index
  // of the CropTarget is used, and translated by JS code back into
  // the CropTarget it had stored.
  bool CropTo(const std::string& crop_target,
              Frame frame,
              Track track = Track::kOriginal) {
    std::string script_result =
        content::EvalJs(web_contents->GetPrimaryMainFrame(),
                        base::StringPrintf("cropToByIndex('%s', '%s', '%s');",
                                           crop_target.c_str(), ToString(frame),
                                           ToString(track)))
            .ExtractString();

    if (frame == Frame::kTopLevelDocument) {
      EXPECT_EQ(0u, script_result.rfind("top-level-", 0)) << script_result;
      return script_result == "top-level-crop-success";
    }
    if (frame == Frame::kEmbeddedFrame) {
      EXPECT_EQ(0u, script_result.rfind("embedded-", 0)) << script_result;
      return script_result == "embedded-crop-success";
    }
    return false;
  }

  bool CloneTrack() {
    std::string script_result =
        content::EvalJs(web_contents->GetPrimaryMainFrame(), "clone();")
            .ExtractString();
    DCHECK(script_result == "clone-track-success" ||
           script_result == "clone-track-failure");
    return script_result == "clone-track-success";
  }

  bool Deallocate(Track track) {
    std::string script_result =
        content::EvalJs(
            web_contents->GetPrimaryMainFrame(),
            base::StringPrintf("deallocate('%s');", ToString(track)))
            .ExtractString();
    DCHECK(script_result == "deallocate-failure" ||
           script_result == "deallocate-success");
    return script_result == "deallocate-success";
  }

  bool HideElement(const char* element_id) {
    std::string script_result =
        content::EvalJs(web_contents->GetPrimaryMainFrame(),
                        base::StringPrintf("hideElement('%s');", element_id))
            .ExtractString();
    DCHECK(script_result == "hide-element-failure" ||
           script_result == "hide-element-success");
    return script_result == "hide-element-success";
  }

  bool CreateNewElement(Frame frame, const char* tag, const std::string& id) {
    DCHECK_NE(frame, Frame::kNone);
    std::string script_result =
        content::EvalJs(web_contents->GetPrimaryMainFrame(),
                        base::StrCat({"createNewElement(\"", ToString(frame),
                                      "\", \"", tag, "\", \"", id, "\");"}))
            .ExtractString();

    if (frame == Frame::kEmbeddedFrame) {
      EXPECT_EQ(0u, script_result.rfind("embedded-", 0)) << script_result;
      return script_result == "embedded-new-element-success";
    }
    DCHECK(frame == Frame::kTopLevelDocument);
    EXPECT_EQ(0u, script_result.rfind("top-level-", 0)) << script_result;
    return script_result == "top-level-new-element-success";
  }

  bool CreateNewDivElement(Frame frame, const std::string& id) {
    return CreateNewElement(frame, "div", id);
  }

  raw_ptr<Browser, DanglingUntriaged> browser;
  raw_ptr<WebContents, DanglingUntriaged> web_contents;
  int tab_strip_index;
};

}  // namespace

// Essentially depends on InProcessBrowserTest, but WebRtcTestBase provides
// detection of JS errors.
class RegionCaptureBrowserTest : public WebRtcTestBase {
 public:
  RegionCaptureBrowserTest() = default;
  ~RegionCaptureBrowserTest() override = default;

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
    // TODO(https://crbug.com/1424557): Remove this after fixing feature
    // detection in 0c tab capture path as it'll no longer be needed.
    if constexpr (!BUILDFLAG(IS_CHROMEOS)) {
      command_line->AppendSwitch(switches::kUseGpuInTests);
    }
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
    permissions::PermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(
            permissions::PermissionRequestManager::ACCEPT_ALL);

    *tab_info = {browser(), web_contents,
                 browser()->tab_strip_model()->active_index()};
    tab_info->StartEmbeddingFrame(
        embedded_iframe_server->GetURL(embedded_iframe_document));

    // Start embedding a frame whose sole purpose is to be same-origin
    // across all other documents and therefore allow communication
    // between them all over a shared BroadcastChannel.
    tab_info->SetUpMailman(servers_[kMailmanServer]->GetURL(
        "/webrtc/region_capture_mailman.html"));
  }

  // Set up all (necessary) tabs, loads iframes, and start capturing the
  // relevant tab.
  void SetUpTest(Frame capturing_entity, bool self_capture) {
    // Other page (for other-tab-capture).
    SetUpPage("/webrtc/region_capture_other_main.html",
              servers_[kOtherPageTopLevelDocument].get(),
              "/webrtc/region_capture_other_embedded.html",
              servers_[kOtherPageEmbeddedDocument].get(), &tabs_[kOtherTab]);

    // Main page (for self-capture). Instantiate it second to help it get focus.
    SetUpPage("/webrtc/region_capture_main.html",
              servers_[kMainPageTopLevelDocument].get(),
              "/webrtc/region_capture_embedded.html",
              servers_[kMainPageEmbeddedDocument].get(), &tabs_[kMainTab]);

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
  raw_ptr<base::CommandLine> command_line_ = nullptr;

  // Holds the tabs manipulated by this test.
  TabInfo tabs_[kTabCount];

  // Each page is served from a distinct origin, thereby proving that cropping
  // works irrespective of whether iframes are in/out-of-process.
  std::vector<std::unique_ptr<net::EmbeddedTestServer>> servers_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class RegionCaptureBrowserCropTest
    : public RegionCaptureBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  ~RegionCaptureBrowserCropTest() override = default;

  void TestCanCropToElementTag(const char* tag) {
    TabInfo& tab = tabs_[kMainTab];

    const std::string element_id = base::StrCat({"new_id_", tag});
    ASSERT_TRUE(
        tab.CreateNewElement(Frame::kTopLevelDocument, tag, element_id));
    const std::string crop_target =
        tab.CropTargetFromElement(Frame::kTopLevelDocument, element_id);
    ASSERT_THAT(crop_target, IsExpectedCropTarget("0"));

    EXPECT_TRUE(tab.CropTo(crop_target, Frame::kTopLevelDocument));
  }
};

INSTANTIATE_TEST_SUITE_P(RegionCaptureBrowserCropTestInstantiation,
                         RegionCaptureBrowserCropTest,
                         Values("a",
                                "blockquote",
                                "body",
                                "button",
                                "canvas",
                                "col",
                                "div",
                                "fieldset",
                                "form",
                                "h1",
                                "header",
                                "hr"
                                "iframe",
                                "img",
                                "input",
                                "output",
                                "span",
                                "svg",
                                "video"));

IN_PROC_BROWSER_TEST_P(RegionCaptureBrowserCropTest, CanCropTo) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TestCanCropToElementTag(GetParam());
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropTargetFromElementReturnsValidIdInMainPage) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  EXPECT_THAT(tabs_[kMainTab].CropTargetFromElement(Frame::kTopLevelDocument),
              IsExpectedCropTarget("0"));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropTargetFromElementReturnsValidIdInCrossOriginIframe) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  EXPECT_THAT(tabs_[kMainTab].CropTargetFromElement(Frame::kEmbeddedFrame),
              IsExpectedCropTarget("0"));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToAllowedIfTopLevelCropsToElementInTopLevel) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_target =
      tab.CropTargetFromElement(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_target, IsExpectedCropTarget("0"));
  EXPECT_TRUE(tab.CropTo(crop_target, Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToAllowedIfTopLevelCropsToElementInEmbedded) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_target =
      tab.CropTargetFromElement(Frame::kEmbeddedFrame);
  ASSERT_THAT(crop_target, IsExpectedCropTarget("0"));
  EXPECT_TRUE(tab.CropTo(crop_target, Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToAllowedIfEmbeddedFrameCropsToElementInTopLevel) {
  SetUpTest(Frame::kEmbeddedFrame, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_target =
      tab.CropTargetFromElement(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_target, IsExpectedCropTarget("0"));
  EXPECT_TRUE(tab.CropTo(crop_target, Frame::kEmbeddedFrame));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToAllowedIfEmbeddedFrameCropsToElementInEmbedded) {
  SetUpTest(Frame::kEmbeddedFrame, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_target =
      tab.CropTargetFromElement(Frame::kEmbeddedFrame);
  ASSERT_THAT(crop_target, IsExpectedCropTarget("0"));
  EXPECT_TRUE(tab.CropTo(crop_target, Frame::kEmbeddedFrame));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest, CropToAllowedToUncrop) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_target =
      tab.CropTargetFromElement(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_target, IsExpectedCropTarget("0"));
  ASSERT_TRUE(tab.CropTo(crop_target, Frame::kTopLevelDocument));
  EXPECT_TRUE(tab.CropTo("undefined", Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToForUncroppingAllowedOnUncroppedTracks) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string crop_target =
      tab.CropTargetFromElement(Frame::kTopLevelDocument);
  ASSERT_THAT(crop_target, IsExpectedCropTarget("0"));
  // CropTo(cropTarget) is intentionally not called.
  // Instead, the test immediately calls CropTo(undefined) on a still-uncropped
  // track, attempting to stop cropping when no cropping was ever specified.
  EXPECT_TRUE(tab.CropTo("undefined", Frame::kTopLevelDocument));
}

// The Promise resolves when it's guaranteed that no additional frames will
// be issued with an earlier crop version. That an actual frame be issued
// at all, let alone with the new crop version, is not actually required,
// or else these promises could languish unfulfilled indefinitely.
IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       CropToOfInvisibleElementResolvesInTimelyFashion) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  ASSERT_TRUE(tab.HideElement("div"));

  const std::string crop_target =
      tab.CropTargetFromElement(Frame::kTopLevelDocument, "div");
  ASSERT_THAT(crop_target, IsExpectedCropTarget("0"));
  EXPECT_TRUE(tab.CropTo(crop_target, Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest, MaxCropIdsInTopLevelDocument) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  for (size_t i = 0; i < kMaxCropIdsPerWebContents; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_TRUE(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id));
    const std::string crop_target =
        tab.CropTargetFromElement(Frame::kTopLevelDocument, element_id);
    ASSERT_THAT(crop_target, IsExpectedCropTarget(base::NumberToString(i)));
  }

  // Create one more element - this one won't get a crop-target.
  const std::string element_id =
      ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id));
  EXPECT_EQ(tab.CropTargetFromElement(Frame::kTopLevelDocument, element_id),
            "top-level-produce-crop-target-error");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest, MaxCropIdsInEmbeddedFrame) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  for (size_t i = 0; i < kMaxCropIdsPerWebContents; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_TRUE(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id));
    const std::string crop_target =
        tab.CropTargetFromElement(Frame::kEmbeddedFrame, element_id);
    ASSERT_THAT(crop_target, IsExpectedCropTarget(base::NumberToString(i)));
  }

  // Create one more element - this one won't get a crop-target.
  const std::string element_id =
      ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id));
  EXPECT_EQ(tab.CropTargetFromElement(Frame::kEmbeddedFrame, element_id),
            "embedded-produce-crop-target-error");
}

IN_PROC_BROWSER_TEST_F(RegionCaptureBrowserTest,
                       MaxCropIdsSharedBetweenFramesInTab) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  static_assert(kMaxCropIdsPerWebContents > 1, "");

  // Create (kMaxCropIdsPerWebContents - 1) new elements and assign each a
  // crop-target.
  for (size_t i = 0; i < kMaxCropIdsPerWebContents - 1; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_TRUE(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id));
    const std::string crop_target =
        tab.CropTargetFromElement(Frame::kTopLevelDocument, element_id);
    ASSERT_THAT(crop_target, IsExpectedCropTarget(base::NumberToString(i)));
  }

  // One more in the embedded frame is possible.
  std::string element_id =
      ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents - 1));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id));
  const std::string crop_target =
      tab.CropTargetFromElement(Frame::kEmbeddedFrame, element_id);
  EXPECT_THAT(crop_target, IsExpectedCropTarget(base::NumberToString(
                               kMaxCropIdsPerWebContents - 1)));

  // Create one more element - this one won't get a crop-target.
  element_id = ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id));
  EXPECT_EQ(tab.CropTargetFromElement(Frame::kTopLevelDocument, element_id),
            "top-level-produce-crop-target-error");
  // Neither in the top-level nor in the embedded frame.
  element_id =
      ("new_id_" + base::NumberToString(kMaxCropIdsPerWebContents + 1));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id));
  EXPECT_EQ(tab.CropTargetFromElement(Frame::kEmbeddedFrame, element_id),
            "embedded-produce-crop-target-error");
}

// Tests related to behavior when cloning.
class RegionCaptureClonesBrowserTest : public RegionCaptureBrowserTest {
 public:
  static const std::string kCropTarget0;
  static const std::string kCropTarget1;

  ~RegionCaptureClonesBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("js-flags", "--expose-gc");
    RegionCaptureBrowserTest::SetUpCommandLine(command_line);
  }

  // Has to be called from within the test's body.
  void ManualSetUp() {
    SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
    EXPECT_THAT(
        tabs_[kMainTab].CropTargetFromElement(Frame::kTopLevelDocument, "div"),
        IsExpectedCropTarget(kCropTarget0));
    EXPECT_THAT(tabs_[kMainTab].CropTargetFromElement(Frame::kTopLevelDocument,
                                                      "embedded_frame"),
                IsExpectedCropTarget(kCropTarget1));
  }

  bool CropTo(const std::string& crop_target, Frame frame, Track track) {
    return tabs_[kMainTab].CropTo(crop_target, frame, track);
  }

  bool CloneTrack() { return tabs_[kMainTab].CloneTrack(); }

  bool Deallocate(Track track) { return tabs_[kMainTab].Deallocate(track); }
};

const std::string RegionCaptureClonesBrowserTest::kCropTarget0 = "0";
const std::string RegionCaptureClonesBrowserTest::kCropTarget1 = "1";

// Sanity cloning 1/2.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest,
                       CanCloneUncroppedTracks) {
  ManualSetUp();

  EXPECT_TRUE(CloneTrack());
}

// Sanity cloning 2/2.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest, CanCloneCroppedTracks) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));

  EXPECT_TRUE(CloneTrack());
}

// Restrictions on cloned tracked 1/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest, CannotCropClone) {
  ManualSetUp();

  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kClone));
}

// Restrictions on cloned tracked 2/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest, CannotRecropClone) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kClone));
}

// Restrictions on cloned tracked 3/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest, CannotUncropClone) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(CropTo("undefined", Frame::kTopLevelDocument, Track::kClone));
}

// Restrictions on original track that has a clone 1/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest,
                       CannotCropTrackThatHasClone) {
  ManualSetUp();

  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(
      CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
}

// Restrictions on original track that has a clone 2/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest,
                       CannotRecropTrackThatHasClone) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(
      CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kOriginal));
}

// Restrictions on original track that has a clone 3/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest,
                       CannotUncropTrackThatHasClone) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(CropTo("undefined", Frame::kTopLevelDocument, Track::kOriginal));
}

// Original track becomes unblocked for cropping after clone is GCed 1/3.
// TODO(crbug.com/1353349)  Re-enable for macOS after flakes are resolved.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CanCropOriginalTrackAfterCloneIsGarbageCollected \
  DISABLED_CanCropOriginalTrackAfterCloneIsGarbageCollected
#else
#define MAYBE_CanCropOriginalTrackAfterCloneIsGarbageCollected \
  CanCropOriginalTrackAfterCloneIsGarbageCollected
#endif
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest,
                       MAYBE_CanCropOriginalTrackAfterCloneIsGarbageCollected) {
  ManualSetUp();

  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(
      CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(Deallocate(Track::kClone));

  EXPECT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
}

// Original track becomes unblocked for cropping after clone is GCed 2/3.
// TODO(crbug.com/1353349) Re-enable after flakes are resolved.
IN_PROC_BROWSER_TEST_F(
    RegionCaptureClonesBrowserTest,
    DISABLED_CanRecropOriginalTrackAfterCloneIsGarbageCollected) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(
      CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(Deallocate(Track::kClone));

  EXPECT_TRUE(CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kOriginal));
}

// Original track becomes unblocked for cropping after clone is GCed 3/3.
IN_PROC_BROWSER_TEST_F(
    RegionCaptureClonesBrowserTest,
    // TODO(crbug.com/1356788): Re-enable this test
    DISABLED_CanUncropOriginalTrackAfterCloneIsGarbageCollected) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(CropTo("undefined", Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(Deallocate(Track::kClone));

  EXPECT_TRUE(CropTo("undefined", Frame::kTopLevelDocument, Track::kOriginal));
}

// The following tests are disabled because of a loosely-related issue,
// where an original track is kept alive if a clone exists, but not vice versa.
// TODO(crbug.com/1333594): Uncomment after fixing the aforementioned issue.
#if 0
// Cloned track becomes unblocked for cropping after original is GCed 1/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest,
                       CanCropCloneAfterOriginalTrackIsGarbageCollected) {
  ManualSetUp();

  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kClone));
  ASSERT_TRUE(Deallocate(Track::kOriginal));

  EXPECT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kClone));
}

// Cloned track becomes unblocked for cropping after original is GCed 2/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest,
                       CanRecropCloneAfterOriginalTrackIsGarbageCollected) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kClone));
  ASSERT_TRUE(Deallocate(Track::kOriginal));

  EXPECT_TRUE(CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kClone));
}

// Cloned track becomes unblocked for cropping after original is GCed 3/3.
IN_PROC_BROWSER_TEST_F(RegionCaptureClonesBrowserTest,
                       CanUncropCloneAfterOriginalTrackIsGarbageCollected) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget0, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(CropTo("undefined", Frame::kTopLevelDocument, Track::kClone));
  ASSERT_TRUE(Deallocate(Track::kOriginal));

  EXPECT_TRUE(CropTo("undefined", Frame::kTopLevelDocument, Track::kClone));
}
#endif

// Tests similar issues to cloning, but with multiple captures triggering
// these issues instead.
class RegionCaptureMultiCaptureBrowserTest
    : public RegionCaptureClonesBrowserTest,
      public WithParamInterface<Frame> {
 public:
  RegionCaptureMultiCaptureBrowserTest() : second_capture_frame_(GetParam()) {}
  ~RegionCaptureMultiCaptureBrowserTest() override = default;

  // Has to be called from within the test's body.
  void ManualSetUp() {
    SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
    EXPECT_THAT(
        tabs_[kMainTab].CropTargetFromElement(Frame::kTopLevelDocument, "div"),
        IsExpectedCropTarget(kCropTarget0));
    EXPECT_THAT(tabs_[kMainTab].CropTargetFromElement(Frame::kTopLevelDocument,
                                                      "embedded_frame"),
                IsExpectedCropTarget(kCropTarget1));
  }

  bool StartSecondCapture() {
    return tabs_[kMainTab].StartSecondCapture(second_capture_frame_);
  }

  bool StopCapture(Track track) {
    return tabs_[kMainTab].StopCapture(Frame::kTopLevelDocument, track);
  }

  const Frame second_capture_frame_;
};

INSTANTIATE_TEST_SUITE_P(_,
                         RegionCaptureMultiCaptureBrowserTest,
                         Values(Frame::kTopLevelDocument,
                                Frame::kEmbeddedFrame));

IN_PROC_BROWSER_TEST_P(RegionCaptureMultiCaptureBrowserTest,
                       CanSelfCaptureAgainIfNeverCropped) {
  ManualSetUp();

  EXPECT_TRUE(StartSecondCapture());
}

IN_PROC_BROWSER_TEST_P(RegionCaptureMultiCaptureBrowserTest,
                       CannotSelfCaptureAgainIfCropped) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kOriginal));

  EXPECT_FALSE(StartSecondCapture());
}

IN_PROC_BROWSER_TEST_P(RegionCaptureMultiCaptureBrowserTest,
                       CannotSelfCaptureAgainIfCroppedAndUncropped) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(CropTo("undefined", Frame::kTopLevelDocument, Track::kOriginal));

  EXPECT_FALSE(StartSecondCapture());
}

IN_PROC_BROWSER_TEST_P(RegionCaptureMultiCaptureBrowserTest,
                       CanSelfCaptureAgainIfCroppedSessionStopped) {
  ManualSetUp();

  ASSERT_TRUE(CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(StopCapture(Track::kOriginal));

  EXPECT_TRUE(StartSecondCapture());
}

IN_PROC_BROWSER_TEST_P(RegionCaptureMultiCaptureBrowserTest,
                       CannotCropIfMultipleSelfCaptureSessionsExist) {
  ManualSetUp();

  ASSERT_TRUE(StartSecondCapture());

  EXPECT_FALSE(
      CropTo(kCropTarget1, Frame::kTopLevelDocument, Track::kOriginal));
  EXPECT_FALSE(CropTo(kCropTarget1, second_capture_frame_, Track::kSecond));
}

// Suite of tests ensuring that only self-capture may crop, and that it may
// only crop to targets in its own tab, but that any target in its own tab
// is permitted.
class RegionCaptureSelfCaptureOnlyBrowserTest
    : public RegionCaptureBrowserTest,
      public WithParamInterface<std::tuple<Frame, bool, Tab, Frame>> {
 public:
  RegionCaptureSelfCaptureOnlyBrowserTest()
      : capturing_entity_(std::get<0>(GetParam())),
        self_capture_(std::get<1>(GetParam())),
        target_element_tab_(std::get<2>(GetParam())),
        target_frame_(std::get<3>(GetParam())) {}
  ~RegionCaptureSelfCaptureOnlyBrowserTest() override = default;

 protected:
  // The capture is done from kMainTab in all instances of this parameterized
  // test. |capturing_entity_| controls whether the capture is initiated
  // from the top-level document of said tab, or an embedded frame.
  const Frame capturing_entity_;

  // Whether capturing self, or capturing the other tab.
  const bool self_capture_;

  // Whether the element on whose crop-target we'll call cropTo():
  // * |target_element_tab_| - whether it's in kMainTab or in kOtherTab.
  // * |target_frame_| - whether it's in the top-level or an embedded frame.
  const Tab target_element_tab_;
  const Frame target_frame_;  // Top-level or embedded frame.
};

std::string ParamsToString(
    const TestParamInfo<RegionCaptureSelfCaptureOnlyBrowserTest::ParamType>&
        info) {
  return base::StrCat(
      {std::get<0>(info.param) == Frame::kTopLevelDocument ? "TopLevel"
                                                           : "EmbeddedFrame",
       std::get<1>(info.param) ? "SelfCapturing" : "CapturingOtherTab",
       "AndCroppingToElementIn",
       std::get<2>(info.param) == kMainTab ? "OwnTabs" : "OtherTabs",
       std::get<3>(info.param) == Frame::kTopLevelDocument ? "TopLevel"
                                                           : "EmbeddedFrame"});
}

INSTANTIATE_TEST_SUITE_P(
    _,
    RegionCaptureSelfCaptureOnlyBrowserTest,
    Combine(Values(Frame::kTopLevelDocument, Frame::kEmbeddedFrame),
            Bool(),
            Values(kMainTab, kOtherTab),
            Values(Frame::kTopLevelDocument, Frame::kEmbeddedFrame)),
    ParamsToString);

IN_PROC_BROWSER_TEST_P(RegionCaptureSelfCaptureOnlyBrowserTest, CropTo) {
  SetUpTest(capturing_entity_, self_capture_);

  // Prevent test false-positive - ensure that both tabs participating in the
  // test have at least one associated crop-target, or otherwise they would not
  // have a CropIdWebContentsHelper.
  // To make things even clearer, ensure both the top-level and the embedded
  // frame have produced crop-targets. (This should not be necessary, but is
  // done as an extra buffer against false-positives.)
  ASSERT_THAT(tabs_[kMainTab].CropTargetFromElement(Frame::kTopLevelDocument),
              IsExpectedCropTarget("0"));
  ASSERT_THAT(tabs_[kMainTab].CropTargetFromElement(Frame::kEmbeddedFrame),
              IsExpectedCropTarget("1"));
  ASSERT_THAT(tabs_[kOtherTab].CropTargetFromElement(Frame::kTopLevelDocument),
              IsExpectedCropTarget("2"));
  ASSERT_THAT(tabs_[kOtherTab].CropTargetFromElement(Frame::kEmbeddedFrame),
              IsExpectedCropTarget("3"));

  const std::string crop_target =
      tabs_[target_element_tab_].CropTargetFromElement(target_frame_);
  ASSERT_THAT(crop_target, IsExpectedCropTarget("4"));

  // Cropping only permitted if both conditions hold.
  const bool expect_permitted =
      (self_capture_ && target_element_tab_ == kMainTab);

  EXPECT_EQ(expect_permitted,
            tabs_[kMainTab].CropTo(crop_target, capturing_entity_));
}

#endif  //  !BUILDFLAG(IS_CHROMEOS_LACROS)
