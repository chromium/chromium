// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gl/gl_switches.h"

// TODO(crbug.com/40184242): Enable this test suite on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using ::content::WebContents;
using ::media::mojom::SubCaptureTargetType;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::TestParamInfo;
using ::testing::Values;
using ::testing::WithParamInterface;

// Comparisons with sub-capture targets (CropTargets/RestrictionTargets) are
// limited. We can only know that a one was returned by the test. However, we
// keep this matcher around in anticipation of targets being made either
// comparable or stringifiable. Then we can do more interesting comparisons,
// like ensuring uniqueness or checking that repeatedly calling
// CropTarget.fromElement() or RestrictionTarget.fromElement() on the same
// Element yields either the same target, or an equivalent one.
MATCHER_P(IsExpectedTarget, expected_target_index, "") {
  static_assert(std::is_same<decltype(arg), const std::string&>::value, "");
  return arg == expected_target_index;
}

const char kMainPageTitle[] = "Region/Element Capture Test - Page 1 (Main)";
const char kOtherPageTitle[] = "Region/Element Capture Test - Page 2 (Main)";

// Tracks SubCaptureTargetIdWebContentsHelper::kMaxIdsPerWebContents.
constexpr size_t kMaxIdsPerWebContents = 100;

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
  switch (frame) {
    case Frame::kNone:
      return "none";
    case Frame::kTopLevelDocument:
      return "top-level";
    case Frame::kEmbeddedFrame:
      return "embedded";
  }
  NOTREACHED();
}

const char* ToString(Track track) {
  switch (track) {
    case Track::kOriginal:
      return "original";
    case Track::kClone:
      return "clone";
    case Track::kSecond:
      return "second";
  }
  NOTREACHED();
}

const char* ToString(SubCaptureTargetType type) {
  switch (type) {
    case SubCaptureTargetType::kCropTarget:
      return "crop-target";
    case SubCaptureTargetType::kRestrictionTarget:
      return "restriction-target";
  }
  NOTREACHED();
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

  std::string SubCaptureTargetFromElement(
      SubCaptureTargetType type,
      Frame frame,
      const std::string& element_id = "div") {
    CHECK_NE(frame, Frame::kNone);

    const std::string js_instruction =
        base::StringPrintf("subCaptureTargetFromElement('%s', '%s', '%s');",
                           ToString(type), ToString(frame), element_id.c_str());

    return content::EvalJs(web_contents->GetPrimaryMainFrame(), js_instruction)
        .ExtractString();
  }

  // Takes as input either the sub-capture target[*], or "undefined" if the test
  // wants track.cropTo(undefined) or track.restrictTo(undefined) to be invoked.
  //
  // [*] Actually, because CropTargets/RestrictionTargets are not stringifiable,
  // an index of the CropTarget/RestrictionTarget is used, and translated by JS
  // code back into the token it had previously stored.
  bool ApplySubCaptureTarget(const std::string& target,
                             SubCaptureTargetType sub_capture_type,
                             Frame frame,
                             Track track = Track::kOriginal) {
    CHECK(frame == Frame::kTopLevelDocument || frame == Frame::kEmbeddedFrame);

    std::string script_result =
        content::EvalJs(web_contents->GetPrimaryMainFrame(),
                        base::StringPrintf(
                            "applySubCaptureByIndex('%s', '%s', '%s', '%s');",
                            target.c_str(), ToString(sub_capture_type),
                            ToString(frame), ToString(track)))
            .ExtractString();

    EXPECT_EQ(0u, script_result.rfind(ToString(frame), 0)) << script_result;
    return script_result == base::StringPrintf("%s-%s-success", ToString(frame),
                                               ToString(sub_capture_type));
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

  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser;
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> web_contents;
  int tab_strip_index;
};

}  // namespace

// Essentially depends on InProcessBrowserTest, but WebRtcTestBase provides
// detection of JS errors.
class SubCaptureBrowserTestBase : public WebRtcTestBase {
 public:
  SubCaptureBrowserTestBase() = default;
  ~SubCaptureBrowserTestBase() override = default;

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
    // MSan and GL do not get along so avoid using the GPU with MSan.
    // TODO(crbug.com/40260482): Remove this after fixing feature
    // detection in 0c tab capture path as it'll no longer be needed.
#if !BUILDFLAG(IS_CHROMEOS) && !defined(MEMORY_SANITIZER)
    command_line->AppendSwitch(switches::kUseGpuInTests);
#endif
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
    tab_info->SetUpMailman(
        servers_[kMailmanServer]->GetURL("/webrtc/sub_capture_mailman.html"));
  }

  // Set up all (necessary) tabs, loads iframes, and start capturing the
  // relevant tab.
  void SetUpTest(Frame capturing_entity, bool self_capture) {
    // Other page (for other-tab-capture).
    SetUpPage("/webrtc/sub_capture_other_main.html",
              servers_[kOtherPageTopLevelDocument].get(),
              "/webrtc/sub_capture_other_embedded.html",
              servers_[kOtherPageEmbeddedDocument].get(), &tabs_[kOtherTab]);

    // Main page (for self-capture). Instantiate it second to help it get focus.
    SetUpPage("/webrtc/sub_capture_main.html",
              servers_[kMainPageTopLevelDocument].get(),
              "/webrtc/sub_capture_embedded.html",
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

  // Each page is served from a distinct origin, thereby proving that
  // cropping/restricting works irrespective of whether iframes are
  // in/out-of-process.
  std::vector<std::unique_ptr<net::EmbeddedTestServer>> servers_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that cropTo() and restrictTo() work on some expected element types.
class SubCaptureBrowserTargetElementTest
    : public SubCaptureBrowserTestBase,
      public WithParamInterface<std::tuple<SubCaptureTargetType, const char*>> {
 public:
  SubCaptureBrowserTargetElementTest()
      : sub_capture_type_(std::get<0>(GetParam())),
        target_element_(std::get<1>(GetParam())) {}
  ~SubCaptureBrowserTargetElementTest() override = default;

  void TestCanApplyToElementTag(const char* tag) {
    TabInfo& tab = tabs_[kMainTab];

    const std::string element_id = base::StrCat({"new_id_", tag});
    ASSERT_TRUE(
        tab.CreateNewElement(Frame::kTopLevelDocument, tag, element_id));
    const std::string target = tab.SubCaptureTargetFromElement(
        sub_capture_type_, Frame::kTopLevelDocument, element_id);
    ASSERT_THAT(target, IsExpectedTarget("0"));

    EXPECT_TRUE(tab.ApplySubCaptureTarget(target, sub_capture_type_,
                                          Frame::kTopLevelDocument));
  }

 protected:
  const SubCaptureTargetType sub_capture_type_;
  const char* const target_element_;
};

std::string SubCaptureBrowserTargetElementTestParamsToString(
    const TestParamInfo<SubCaptureBrowserTargetElementTest::ParamType>& info) {
  const std::string api =
      std::get<0>(info.param) == SubCaptureTargetType::kCropTarget
          ? "RegionCapture"
          : "ElementCapture";

  std::string element = std::get<1>(info.param);
  element[0] = std::toupper(element[0]);

  return api + element;
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SubCaptureBrowserTargetElementTest,
    Combine(Values(SubCaptureTargetType::kCropTarget,
                   SubCaptureTargetType::kRestrictionTarget),
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
                   "video")),
    SubCaptureBrowserTargetElementTestParamsToString);

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTargetElementTest, ApplyToElement) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TestCanApplyToElementTag(target_element_);
}

class SubCaptureBrowserTest : public SubCaptureBrowserTestBase,
                              public WithParamInterface<SubCaptureTargetType> {
 public:
  SubCaptureBrowserTest() : type_(GetParam()) {}
  ~SubCaptureBrowserTest() override = default;

 protected:
  const SubCaptureTargetType type_;
};

std::string SubCaptureBrowserTestParamsToString(
    const TestParamInfo<SubCaptureBrowserTest::ParamType>& info) {
  return info.param == SubCaptureTargetType::kCropTarget ? "RegionCapture"
                                                         : "ElementCapture";
}

INSTANTIATE_TEST_SUITE_P(,
                         SubCaptureBrowserTest,
                         Values(SubCaptureTargetType::kCropTarget,
                                SubCaptureTargetType::kRestrictionTarget),
                         SubCaptureBrowserTestParamsToString);

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest,
                       FromElementReturnsValidIdInMainPage) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  EXPECT_THAT(tabs_[kMainTab].SubCaptureTargetFromElement(
                  type_, Frame::kTopLevelDocument),
              IsExpectedTarget("0"));
}

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest,
                       FromElementReturnsValidIdInCrossOriginIframe) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  EXPECT_THAT(
      tabs_[kMainTab].SubCaptureTargetFromElement(type_, Frame::kEmbeddedFrame),
      IsExpectedTarget("0"));
}

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest,
                       SubCaptureAllowedIfTopLevelAppliesToElementInTopLevel) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string target =
      tab.SubCaptureTargetFromElement(type_, Frame::kTopLevelDocument);
  ASSERT_THAT(target, IsExpectedTarget("0"));
  EXPECT_TRUE(
      tab.ApplySubCaptureTarget(target, type_, Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest,
                       SubCaptureAllowedIfTopLevelAppliesToElementInEmbedded) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string target =
      tab.SubCaptureTargetFromElement(type_, Frame::kEmbeddedFrame);
  ASSERT_THAT(target, IsExpectedTarget("0"));
  EXPECT_TRUE(
      tab.ApplySubCaptureTarget(target, type_, Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_P(
    SubCaptureBrowserTest,
    SubCaptureAllowedIfEmbeddedFrameAppliesToElementInTopLevel) {
  SetUpTest(Frame::kEmbeddedFrame, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string target =
      tab.SubCaptureTargetFromElement(type_, Frame::kTopLevelDocument);
  ASSERT_THAT(target, IsExpectedTarget("0"));
  EXPECT_TRUE(tab.ApplySubCaptureTarget(target, type_, Frame::kEmbeddedFrame));
}

IN_PROC_BROWSER_TEST_P(
    SubCaptureBrowserTest,
    SubCaptureAllowedIfEmbeddedFrameAppliesToElementInEmbedded) {
  SetUpTest(Frame::kEmbeddedFrame, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string target =
      tab.SubCaptureTargetFromElement(type_, Frame::kEmbeddedFrame);
  ASSERT_THAT(target, IsExpectedTarget("0"));
  EXPECT_TRUE(tab.ApplySubCaptureTarget(target, type_, Frame::kEmbeddedFrame));
}

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest, SubCaptureAllowedToUndo) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string target =
      tab.SubCaptureTargetFromElement(type_, Frame::kTopLevelDocument);
  ASSERT_THAT(target, IsExpectedTarget("0"));
  ASSERT_TRUE(
      tab.ApplySubCaptureTarget(target, type_, Frame::kTopLevelDocument));
  EXPECT_TRUE(
      tab.ApplySubCaptureTarget("undefined", type_, Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_P(
    SubCaptureBrowserTest,
    MayAttemptToUndoSubCaptureOnTracksWhereSubCaptureHasNotBeenApplied) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  const std::string target =
      tab.SubCaptureTargetFromElement(type_, Frame::kTopLevelDocument);
  ASSERT_THAT(target, IsExpectedTarget("0"));
  // ApplySubCaptureTarget(target) is intentionally not called.
  // Instead, the test immediately calls ApplySubCaptureTarget(undefined) on a
  // still-uncropped-and-unrestricted track, attempting to undo sub-capture
  // although it's not been applied.
  EXPECT_TRUE(
      tab.ApplySubCaptureTarget("undefined", type_, Frame::kTopLevelDocument));
}

// The Promise resolves when it's guaranteed that no additional frames will be
// issued with an earlier sub-capture-target version. That an actual frame be
// issued at all, let alone with the new sub-capture-target version, is not
// actually required, or else these promises could languish unfulfilled
// indefinitely.
IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest,
                       SubCaptureOfInvisibleElementResolvesInTimelyFashion) {
  SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
  TabInfo& tab = tabs_[kMainTab];

  ASSERT_TRUE(tab.HideElement("div"));

  const std::string target =
      tab.SubCaptureTargetFromElement(type_, Frame::kTopLevelDocument, "div");
  ASSERT_THAT(target, IsExpectedTarget("0"));
  EXPECT_TRUE(
      tab.ApplySubCaptureTarget(target, type_, Frame::kTopLevelDocument));
}

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest, MaxIdsInTopLevelDocument) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  for (size_t i = 0; i < kMaxIdsPerWebContents; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_TRUE(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id));
    const std::string target = tab.SubCaptureTargetFromElement(
        type_, Frame::kTopLevelDocument, element_id);
    ASSERT_THAT(target, IsExpectedTarget(base::NumberToString(i)));
  }

  // Create one more element - this one won't get a sub-capture-target.
  const std::string element_id =
      ("new_id_" + base::NumberToString(kMaxIdsPerWebContents));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id));
  EXPECT_EQ(tab.SubCaptureTargetFromElement(type_, Frame::kTopLevelDocument,
                                            element_id),
            base::StringPrintf("top-level-produce-%s-error", ToString(type_)));
}

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest, MaxIdsInEmbeddedFrame) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  for (size_t i = 0; i < kMaxIdsPerWebContents; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_TRUE(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id));
    const std::string target = tab.SubCaptureTargetFromElement(
        type_, Frame::kEmbeddedFrame, element_id);
    ASSERT_THAT(target, IsExpectedTarget(base::NumberToString(i)));
  }

  // Create one more element - this one won't get a sub-capture-target.
  const std::string element_id =
      ("new_id_" + base::NumberToString(kMaxIdsPerWebContents));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id));
  EXPECT_EQ(
      tab.SubCaptureTargetFromElement(type_, Frame::kEmbeddedFrame, element_id),
      base::StringPrintf("embedded-produce-%s-error", ToString(type_)));
}

IN_PROC_BROWSER_TEST_P(SubCaptureBrowserTest, MaxIdsSharedBetweenFramesInTab) {
  SetUpTest(Frame::kNone, /*self_capture=*/false);
  TabInfo& tab = tabs_[kMainTab];

  static_assert(kMaxIdsPerWebContents > 1, "");

  // Create (kMaxIdsPerWebContents - 1) new elements and assign each a
  // sub-capture-target.
  for (size_t i = 0; i < kMaxIdsPerWebContents - 1; ++i) {
    const std::string element_id = ("new_id_" + base::NumberToString(i));
    ASSERT_TRUE(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id));
    const std::string target = tab.SubCaptureTargetFromElement(
        type_, Frame::kTopLevelDocument, element_id);
    ASSERT_THAT(target, IsExpectedTarget(base::NumberToString(i)));
  }

  // One more in the embedded frame is possible.
  std::string element_id =
      ("new_id_" + base::NumberToString(kMaxIdsPerWebContents - 1));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id));
  const std::string target =
      tab.SubCaptureTargetFromElement(type_, Frame::kEmbeddedFrame, element_id);
  EXPECT_THAT(target, IsExpectedTarget(
                          base::NumberToString(kMaxIdsPerWebContents - 1)));

  // Create one more element - this one won't get a sub-capture-target.
  element_id = ("new_id_" + base::NumberToString(kMaxIdsPerWebContents));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kTopLevelDocument, element_id));
  EXPECT_EQ(tab.SubCaptureTargetFromElement(type_, Frame::kTopLevelDocument,
                                            element_id),
            base::StringPrintf("top-level-produce-%s-error", ToString(type_)));
  // Neither in the top-level nor in the embedded frame.
  element_id = ("new_id_" + base::NumberToString(kMaxIdsPerWebContents + 1));
  ASSERT_TRUE(tab.CreateNewDivElement(Frame::kEmbeddedFrame, element_id));
  EXPECT_EQ(
      tab.SubCaptureTargetFromElement(type_, Frame::kEmbeddedFrame, element_id),
      base::StringPrintf("embedded-produce-%s-error", ToString(type_)));
}

// Tests related to behavior when cloning.
class SubCaptureClonesBrowserTestBase : public SubCaptureBrowserTestBase {
 public:
  static constexpr char kTarget0[] = "0";
  static constexpr char kTarget1[] = "1";

  ~SubCaptureClonesBrowserTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("js-flags", "--expose-gc");
    SubCaptureBrowserTestBase::SetUpCommandLine(command_line);
  }

  // Has to be called from within the test's body.
  void ManualSetUp(SubCaptureTargetType type) {
    SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
    EXPECT_THAT(tabs_[kMainTab].SubCaptureTargetFromElement(
                    type, Frame::kTopLevelDocument, "div"),
                IsExpectedTarget(kTarget0));
    EXPECT_THAT(tabs_[kMainTab].SubCaptureTargetFromElement(
                    type, Frame::kTopLevelDocument, "embedded_frame"),
                IsExpectedTarget(kTarget1));
  }

  bool ApplySubCaptureTarget(SubCaptureTargetType type,
                             const std::string& target,
                             Frame frame,
                             Track track) {
    return tabs_[kMainTab].ApplySubCaptureTarget(target, type, frame, track);
  }

  bool CloneTrack() { return tabs_[kMainTab].CloneTrack(); }

  bool Deallocate(Track track) { return tabs_[kMainTab].Deallocate(track); }
};

class SubCaptureClonesBrowserTest
    : public SubCaptureClonesBrowserTestBase,
      public WithParamInterface<SubCaptureTargetType> {
 public:
  SubCaptureClonesBrowserTest() : type_(GetParam()) {}
  ~SubCaptureClonesBrowserTest() override = default;

 protected:
  const SubCaptureTargetType type_;
};

std::string SubCaptureClonesBrowserTestParamsToString(
    const TestParamInfo<SubCaptureClonesBrowserTest::ParamType>& info) {
  return info.param == SubCaptureTargetType::kCropTarget ? "RegionCapture"
                                                         : "ElementCapture";
}

INSTANTIATE_TEST_SUITE_P(,
                         SubCaptureClonesBrowserTest,
                         Values(SubCaptureTargetType::kCropTarget,
                                SubCaptureTargetType::kRestrictionTarget),
                         SubCaptureClonesBrowserTestParamsToString);

// Sanity cloning 1/2.
IN_PROC_BROWSER_TEST_P(SubCaptureClonesBrowserTest,
                       CanCloneTracksWhichHadNoSubCaptureApplied) {
  ManualSetUp(type_);

  EXPECT_TRUE(CloneTrack());
}

// Sanity cloning 2/2.
IN_PROC_BROWSER_TEST_P(SubCaptureClonesBrowserTest,
                       CanCloneTrackWhichHaveSubCaptureApplied) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));

  EXPECT_TRUE(CloneTrack());
}

// Restrictions on cloned tracks 1/3.
IN_PROC_BROWSER_TEST_P(SubCaptureClonesBrowserTest,
                       CannotApplySubCaptureOnClones) {
  ManualSetUp(type_);

  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                     Track::kClone));
}

// Restrictions on cloned tracks 2/3.
IN_PROC_BROWSER_TEST_P(SubCaptureClonesBrowserTest,
                       CannotReapplySubCaptureOnClones) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                     Track::kClone));
}

// Restrictions on cloned tracks 3/3.
IN_PROC_BROWSER_TEST_P(SubCaptureClonesBrowserTest,
                       CannotUndoSubCaptureOnClones) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(ApplySubCaptureTarget(type_, "undefined",
                                     Frame::kTopLevelDocument, Track::kClone));
}

// Restrictions on original track that has a clone 1/3.
IN_PROC_BROWSER_TEST_P(SubCaptureClonesBrowserTest,
                       CannotApplySubCaptureOnTracksThatHaveClones) {
  ManualSetUp(type_);

  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                     Track::kOriginal));
}

// Restrictions on original track that has a clone 2/3.
IN_PROC_BROWSER_TEST_P(SubCaptureClonesBrowserTest,
                       CannotReapplySubCaptureOnTracksThatHaveClones) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                     Track::kOriginal));
}

// Restrictions on original track that has a clone 3/3.
IN_PROC_BROWSER_TEST_P(SubCaptureClonesBrowserTest,
                       CannotUndoSubCaptureOnTracksThatHaveClones) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(CloneTrack());

  EXPECT_FALSE(ApplySubCaptureTarget(
      type_, "undefined", Frame::kTopLevelDocument, Track::kOriginal));
}

// Original track becomes unblocked for sub-capture after clone is GCed 1/3.
//
// TODO(crbug.com/40858400)  Re-enable for macOS and ChromeOS after flakes are
// resolved.
// TODO(crbug.com/40919381): Also flakes on linux-bfcache-rel, so turning the
// test off entirely.
IN_PROC_BROWSER_TEST_P(
    SubCaptureClonesBrowserTest,
    DISABLED_CanApplySubCaptureOnOriginalTrackAfterCloneIsGarbageCollected) {
  ManualSetUp(type_);

  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                     Track::kOriginal));
  ASSERT_TRUE(Deallocate(Track::kClone));

  EXPECT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
}

// Original track becomes unblocked for sub-capture after clone is GCed 2/3.
//
// TODO(crbug.com/40858400) Re-enable after flakes are resolved.
IN_PROC_BROWSER_TEST_P(
    SubCaptureClonesBrowserTest,
    DISABLED_CanReapplySubCaptureOnOriginalTrackAfterCloneIsGarbageCollected) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                     Track::kOriginal));
  ASSERT_TRUE(Deallocate(Track::kClone));

  EXPECT_TRUE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                    Track::kOriginal));
}

// Original track becomes unblocked for sub-capture clone is GCed 3/3.
//
// TODO(crbug.com/40860614): Re-enable this test.
IN_PROC_BROWSER_TEST_P(
    SubCaptureClonesBrowserTest,
    DISABLED_CanUndoSubCaptureOnOriginalTrackAfterCloneIsGarbageCollected) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(ApplySubCaptureTarget(
      type_, "undefined", Frame::kTopLevelDocument, Track::kOriginal));
  ASSERT_TRUE(Deallocate(Track::kClone));

  EXPECT_TRUE(ApplySubCaptureTarget(
      type_, "undefined", Frame::kTopLevelDocument, Track::kOriginal));
}

// Cloned track becomes unblocked for sub-capture after original is GCed 1/3.
//
// The following test is disabled because of a loosely-related issue,
// where an original track is kept alive if a clone exists, but not vice versa.
// TODO(crbug.com/40845775): Uncomment after fixing the aforementioned issue.
IN_PROC_BROWSER_TEST_P(
    SubCaptureClonesBrowserTest,
    DISABLED_CanApplySubCaptureOnCloneAfterOriginalTrackIsGarbageCollected) {
  ManualSetUp(type_);

  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                     Track::kClone));
  ASSERT_TRUE(Deallocate(Track::kOriginal));

  EXPECT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kClone));
}

// Cloned track becomes unblocked for sub-capture after original is GCed 2/3.
//
// The following test is disabled because of a loosely-related issue,
// where an original track is kept alive if a clone exists, but not vice versa.
// TODO(crbug.com/40845775): Uncomment after fixing the aforementioned issue.
IN_PROC_BROWSER_TEST_P(
    SubCaptureClonesBrowserTest,
    DISABLED_CanReapplySubCaptureOnCloneAfterOriginalTrackIsGarbageCollected) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                     Track::kClone));
  ASSERT_TRUE(Deallocate(Track::kOriginal));

  EXPECT_TRUE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                    Track::kClone));
}

// Cloned track becomes unblocked for sub-capture after original is GCed 3/3.
//
// The following test is disabled because of a loosely-related issue,
// where an original track is kept alive if a clone exists, but not vice versa.
// TODO(crbug.com/40845775): Uncomment after fixing the aforementioned issue.
IN_PROC_BROWSER_TEST_P(
    SubCaptureClonesBrowserTest,
    DISABLED_CanUndoSubCaptureOnCloneAfterOriginalTrackIsGarbageCollected) {
  ManualSetUp(type_);

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget0, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(CloneTrack());
  ASSERT_FALSE(ApplySubCaptureTarget(type_, "undefined",
                                     Frame::kTopLevelDocument, Track::kClone));
  ASSERT_TRUE(Deallocate(Track::kOriginal));

  EXPECT_TRUE(ApplySubCaptureTarget(type_, "undefined",
                                    Frame::kTopLevelDocument, Track::kClone));
}

// Tests similar issues to cloning, but with multiple captures triggering
// these issues instead.
class SubCaptureMultiCaptureBrowserTest
    : public SubCaptureClonesBrowserTestBase,
      public WithParamInterface<std::tuple<SubCaptureTargetType, Frame>> {
 public:
  SubCaptureMultiCaptureBrowserTest()
      : type_(std::get<0>(GetParam())),
        second_capture_frame_(std::get<1>(GetParam())) {}

  ~SubCaptureMultiCaptureBrowserTest() override = default;

  // Has to be called from within the test's body.
  void ManualSetUp() {
    SetUpTest(Frame::kTopLevelDocument, /*self_capture=*/true);
    EXPECT_THAT(tabs_[kMainTab].SubCaptureTargetFromElement(
                    type_, Frame::kTopLevelDocument, "div"),
                IsExpectedTarget(kTarget0));
    EXPECT_THAT(tabs_[kMainTab].SubCaptureTargetFromElement(
                    type_, Frame::kTopLevelDocument, "embedded_frame"),
                IsExpectedTarget(kTarget1));
  }

  bool StartSecondCapture() {
    return tabs_[kMainTab].StartSecondCapture(second_capture_frame_);
  }

  bool StopCapture(Track track) {
    return tabs_[kMainTab].StopCapture(Frame::kTopLevelDocument, track);
  }

 protected:
  const SubCaptureTargetType type_;
  const Frame second_capture_frame_;
};

std::string SubCaptureMultiCaptureBrowserTestParamsToString(
    const TestParamInfo<SubCaptureMultiCaptureBrowserTest::ParamType>& info) {
  const std::string api =
      std::get<0>(info.param) == SubCaptureTargetType::kCropTarget
          ? "RegionCapture"
          : "ElementCapture";

  const std::string frame = std::get<1>(info.param) == Frame::kTopLevelDocument
                                ? "TopLevelFrame"
                                : "EmbeddedFrame";

  return base::StrCat({api, "AndSecondCaptureFrameIs", frame});
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SubCaptureMultiCaptureBrowserTest,
    Combine(Values(SubCaptureTargetType::kCropTarget,
                   SubCaptureTargetType::kRestrictionTarget),
            Values(Frame::kTopLevelDocument, Frame::kEmbeddedFrame)),
    SubCaptureMultiCaptureBrowserTestParamsToString);

IN_PROC_BROWSER_TEST_P(SubCaptureMultiCaptureBrowserTest,
                       CanSelfCaptureAgainIfNeverAppliedSubCapture) {
  ManualSetUp();

  EXPECT_TRUE(StartSecondCapture());
}

IN_PROC_BROWSER_TEST_P(SubCaptureMultiCaptureBrowserTest,
                       CannotSelfCaptureAgainIfAppliedSubCapture) {
  ManualSetUp();

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                    Track::kOriginal));

  EXPECT_FALSE(StartSecondCapture());
}

IN_PROC_BROWSER_TEST_P(SubCaptureMultiCaptureBrowserTest,
                       CannotSelfCaptureAgainIfSubCapturedAppliedAndUnapplied) {
  ManualSetUp();

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(ApplySubCaptureTarget(
      type_, "undefined", Frame::kTopLevelDocument, Track::kOriginal));

  EXPECT_FALSE(StartSecondCapture());
}

IN_PROC_BROWSER_TEST_P(SubCaptureMultiCaptureBrowserTest,
                       CanSelfCaptureAgainIfSubCaptureSessionStopped) {
  ManualSetUp();

  ASSERT_TRUE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                    Track::kOriginal));
  ASSERT_TRUE(StopCapture(Track::kOriginal));

  EXPECT_TRUE(StartSecondCapture());
}

IN_PROC_BROWSER_TEST_P(
    SubCaptureMultiCaptureBrowserTest,
    CannotApplySubCaptureIfMultipleSelfCaptureSessionsExist) {
  ManualSetUp();

  ASSERT_TRUE(StartSecondCapture());

  EXPECT_FALSE(ApplySubCaptureTarget(type_, kTarget1, Frame::kTopLevelDocument,
                                     Track::kOriginal));
  EXPECT_FALSE(ApplySubCaptureTarget(type_, kTarget1, second_capture_frame_,
                                     Track::kSecond));
}

// Suite of tests ensuring that only self-capture sessions may be the target of
// sub-capture, and that the app may only apply sub-capture using targets in its
// own tab. (However, any target in the current tab is permitted.)
class SubCaptureSelfCaptureOnlyBrowserTest
    : public SubCaptureBrowserTestBase,
      public WithParamInterface<
          std::tuple<SubCaptureTargetType, Frame, bool, Tab, Frame>> {
 public:
  SubCaptureSelfCaptureOnlyBrowserTest()
      : type_(std::get<0>(GetParam())),
        capturing_entity_(std::get<1>(GetParam())),
        self_capture_(std::get<2>(GetParam())),
        target_element_tab_(std::get<3>(GetParam())),
        target_frame_(std::get<4>(GetParam())) {}
  ~SubCaptureSelfCaptureOnlyBrowserTest() override = default;

 protected:
  // Whether Region Capture or Element Capture is tested.
  const SubCaptureTargetType type_;

  // The capture is done from kMainTab in all instances of this parameterized
  // test. |capturing_entity_| controls whether the capture is initiated
  // from the top-level document of said tab, or an embedded frame.
  const Frame capturing_entity_;

  // Whether capturing self, or capturing the other tab.
  const bool self_capture_;

  // Determines the the element on whose sub-capture-target we'll call cropTo()
  // or restrictTo():
  // * |target_element_tab_| - whether it's in kMainTab or in kOtherTab.
  // * |target_frame_| - whether it's in the top-level or an embedded frame.
  const Tab target_element_tab_;
  const Frame target_frame_;  // Top-level or embedded frame.
};

std::string SubCaptureSelfCaptureOnlyBrowserTestParamsToString(
    const TestParamInfo<SubCaptureSelfCaptureOnlyBrowserTest::ParamType>&
        info) {
  return base::StrCat(
      {std::get<0>(info.param) == SubCaptureTargetType::kCropTarget
           ? "RegionCapture"
           : "ElementCapture",
       std::get<1>(info.param) == Frame::kTopLevelDocument ? "TopLevel"
                                                           : "EmbeddedFrame",
       std::get<2>(info.param) ? "SelfCapturing" : "CapturingOtherTab",
       "AndApplyingToElementIn",
       std::get<3>(info.param) == kMainTab ? "OwnTabs" : "OtherTabs",
       std::get<4>(info.param) == Frame::kTopLevelDocument ? "TopLevel"
                                                           : "EmbeddedFrame"});
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SubCaptureSelfCaptureOnlyBrowserTest,
    Combine(Values(SubCaptureTargetType::kCropTarget,
                   SubCaptureTargetType::kRestrictionTarget),
            Values(Frame::kTopLevelDocument, Frame::kEmbeddedFrame),
            Bool(),
            Values(kMainTab, kOtherTab),
            Values(Frame::kTopLevelDocument, Frame::kEmbeddedFrame)),
    SubCaptureSelfCaptureOnlyBrowserTestParamsToString);

IN_PROC_BROWSER_TEST_P(SubCaptureSelfCaptureOnlyBrowserTest, ApplySubCapture) {
  SetUpTest(capturing_entity_, self_capture_);

  // Prevent test false-positive - ensure that both tabs participating in the
  // test have at least one associated sub-capture-target, or otherwise they
  // would not have a SubCaptureTargetIdWebContentsHelper.
  // To make things even clearer, ensure both the top-level and the embedded
  // frame have produced sub-capture-targets. (This should not be necessary,
  // but is done as an extra buffer against false-positives.)
  ASSERT_THAT(tabs_[kMainTab].SubCaptureTargetFromElement(
                  type_, Frame::kTopLevelDocument),
              IsExpectedTarget("0"));
  ASSERT_THAT(
      tabs_[kMainTab].SubCaptureTargetFromElement(type_, Frame::kEmbeddedFrame),
      IsExpectedTarget("1"));
  ASSERT_THAT(tabs_[kOtherTab].SubCaptureTargetFromElement(
                  type_, Frame::kTopLevelDocument),
              IsExpectedTarget("2"));
  ASSERT_THAT(tabs_[kOtherTab].SubCaptureTargetFromElement(
                  type_, Frame::kEmbeddedFrame),
              IsExpectedTarget("3"));

  const std::string target =
      tabs_[target_element_tab_].SubCaptureTargetFromElement(type_,
                                                             target_frame_);
  ASSERT_THAT(target, IsExpectedTarget("4"));

  // Apply sub-capture only permitted if both conditions hold.
  const bool expect_permitted =
      (self_capture_ && target_element_tab_ == kMainTab);

  EXPECT_EQ(expect_permitted, tabs_[kMainTab].ApplySubCaptureTarget(
                                  target, type_, capturing_entity_));
}

#endif  //  !BUILDFLAG(IS_CHROMEOS_LACROS)
