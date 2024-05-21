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
#include "ui/gl/gl_switches.h"

using content::WebContents;

namespace {

// The captured tab is identified by its title.
const char kCapturedTabTitle[] = "totally-unique-captured-page-title";

// Capturing page (top-level document).
const char kCapturingPageMain[] = "/webrtc/capturing_page_main.html";
// Capturing page (embedded document).
const char kCapturingPageEmbedded[] = "/webrtc/capturing_page_embedded.html";
// Captured page.
const char kCapturedPageMain[] = "/webrtc/captured_page_main.html";
// Similar contents to kCapturedPageMain, but on a different page, which can
// be served same-origin or cross-origin.
const char kCapturedPageOther[] = "/webrtc/captured_page_other.html";

const char* kArbitraryOrigin = "https://arbitrary-origin.com";
const char* kNoEmbeddedCaptureHandle = "no-embedded-capture-handle";

enum class BrowserType { kRegular, kIncognito };

std::string StringifyPermittedOrigins(
    const std::vector<std::string>& permitted_origins) {
  if (permitted_origins.empty()) {
    return "[]";
  }
  return base::StrCat(
      {"[\"", base::JoinString(permitted_origins, "\", \""), "\"]"});
}

std::string StringifyCaptureHandle(WebContents* web_contents,
                                   bool expose_origin,
                                   const std::string& handle) {
  if (!expose_origin && handle.empty()) {
    return "";
  }

  std::string origin_str;
  if (expose_origin) {
    const auto origin =
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    origin_str =
        base::StringPrintf(",\"origin\":\"%s\"", origin.Serialize().c_str());
  }

  return base::StringPrintf("{\"handle\":\"%s\"%s}", handle.c_str(),
                            origin_str.c_str());
}

// Conveniently pack together a captured tab and the capture-handle that
// is expected to be observed by capturers from a permitted origin.
struct TabInfo {
  void StartCapturing() {
    // Bring the tab into focus. This avoids getDisplayMedia rejection.
    browser->tab_strip_model()->ActivateTabAt(tab_strip_index);

    EXPECT_EQ(content::EvalJs(web_contents->GetPrimaryMainFrame(),
                              "captureOtherTab();"),
              "capture-success");
  }

  void StartCapturingFromEmbeddedFrame() {
    EXPECT_EQ(content::EvalJs(web_contents->GetPrimaryMainFrame(),
                              "captureOtherTabFromEmbeddedFrame();"),
              "embedded-capture-success");
  }

  url::Origin GetOrigin() const {
    return web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  }

  std::string GetOriginAsString() const { return GetOrigin().Serialize(); }

  void SetCaptureHandleConfig(
      bool expose_origin,
      const std::string& handle,
      const std::vector<std::string>& permitted_origins) {
    EXPECT_EQ(content::EvalJs(
                  web_contents->GetPrimaryMainFrame(),
                  base::StringPrintf(
                      "callSetCaptureHandleConfig(%s, \"%s\", %s);",
                      expose_origin ? "true" : "false", handle.c_str(),
                      StringifyPermittedOrigins(permitted_origins).c_str())),
              "capture-handle-set");

    capture_handle =
        StringifyCaptureHandle(web_contents, expose_origin, handle);
  }

  std::string ReadCaptureHandle() {
    return content::EvalJs(web_contents->GetPrimaryMainFrame(),
                           "readCaptureHandle();")
        .ExtractString();
  }

  std::string ReadCaptureHandleInEmbeddedFrame() {
    return content::EvalJs(web_contents->GetPrimaryMainFrame(),
                           "readCaptureHandleInEmbeddedFrame();")
        .ExtractString();
  }

  void Navigate(GURL url, bool expect_handle_reset = false) {
    ASSERT_EQ(content::EvalJs(web_contents->GetPrimaryMainFrame(),
                              base::StringPrintf("clickLinkToUrl(\"%s\");",
                                                 url.spec().c_str())),
              "link-success");

    if (expect_handle_reset) {
      capture_handle = "";
    }
  }

  std::string LastEvent() {
    return content::EvalJs(web_contents->GetPrimaryMainFrame(),
                           "readLastEvent();")
        .ExtractString();
  }

  std::string LastEmbeddedEvent() {
    return content::EvalJs(web_contents->GetPrimaryMainFrame(),
                           "readLastEmbeddedEvent();")
        .ExtractString();
  }

  void StartEmbeddingFrame(const GURL& url) {
    EXPECT_EQ(content::EvalJs(web_contents->GetPrimaryMainFrame(),
                              base::StringPrintf("startEmbeddingFrame('%s');",
                                                 url.spec().c_str())),
              "embedding-done");
  }

  raw_ptr<Browser> browser;
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> web_contents;
  int tab_strip_index;
  std::string capture_handle;  // Expected value for those who may observe.
};

TabInfo MakeTabInfoFromActiveTab(Browser* browser) {
  WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // "POISON_VALUE" intentionally fails comparisons if unset when read.
  return TabInfo{browser, web_contents,
                 browser->tab_strip_model()->active_index(), "POISON_VALUE"};
}

}  // namespace

// Essentially depends on InProcessBrowserTest, but WebRtcTestBase provides
// detection of JS errors.
class CaptureHandleBrowserTest : public WebRtcTestBase {
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
    command_line->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle, kCapturedTabTitle);
    // MSan and GL do not get along so avoid using the GPU with MSan.
    // TODO(crbug.com/40260482): Remove the CrOS exception after fixing feature
    // detection in 0c tab capture path as it'll no longer be needed.
#if !BUILDFLAG(IS_CHROMEOS) && !defined(MEMORY_SANITIZER)
    command_line->AppendSwitch(switches::kUseGpuInTests);
#endif
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
  // a single embedded server is used for all pages.
  WebContents* OpenTestPageInNewTab(Browser* browser,
                                    const std::string& test_page,
                                    net::EmbeddedTestServer* server) const {
    chrome::AddTabAt(browser, GURL(url::kAboutBlankURL), -1, true);
    GURL url = server->GetURL(test_page);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));
    WebContents* new_tab = browser->tab_strip_model()->GetActiveWebContents();
    permissions::PermissionRequestManager::FromWebContents(new_tab)
        ->set_auto_response_for_test(
            permissions::PermissionRequestManager::ACCEPT_ALL);
    return new_tab;
  }

  Browser* GetBrowser(BrowserType browser_type) {
    DCHECK(browser_type == BrowserType::kRegular ||
           browser_type == BrowserType::kIncognito);

    if (browser_type == BrowserType::kRegular) {
      return browser();
    }

    if (!incognito_browser_) {
      incognito_browser_ = CreateIncognitoBrowser();
    }
    return incognito_browser_;
  }

  TabInfo SetUpCapturingPage(bool start_capturing,
                             BrowserType browser_type = BrowserType::kRegular) {
    Browser* const browser = GetBrowser(browser_type);

    OpenTestPageInNewTab(browser, kCapturingPageMain,
                         servers_[kCapturingServer].get());

    auto result = MakeTabInfoFromActiveTab(browser);
    if (start_capturing) {
      result.StartCapturing();
    }

    event_sinks_.push_back(result.web_contents.get());

    return result;
  }

  TabInfo SetUpCapturedPage(bool expose_origin,
                            const std::string& handle,
                            const std::vector<std::string>& permitted_origins,
                            bool self_capture = false,
                            BrowserType browser_type = BrowserType::kRegular) {
    // Normally, the captured page has its own server (=origin) and own file.
    // But if self-capture is tested, use the origin and page of the capturer.
    const char* page = self_capture ? kCapturingPageMain : kCapturedPageMain;
    const int server_index = self_capture ? kCapturingServer : kCapturedServer;

    Browser* const browser = GetBrowser(browser_type);

    auto* const web_contents =
        OpenTestPageInNewTab(browser, page, servers_[server_index].get());

    // The target for getDisplayMedia is determined via the title. If we want
    // the capturing page to capture itself, then it has to change its title.
    if (self_capture) {
      EXPECT_EQ(content::EvalJs(
                    web_contents->GetPrimaryMainFrame(),
                    base::StringPrintf("setTitle(\"%s\");", kCapturedTabTitle)),
                "title-changed");
    }

    auto tab_info = MakeTabInfoFromActiveTab(browser);

    tab_info.SetCaptureHandleConfig(expose_origin, handle, permitted_origins);

    return tab_info;
  }

  enum {
    kCapturedServer,
    kOtherCapturedServer,
    kCapturingServer,          // Top-level document.
    kEmbeddedCapturingServer,  // Embedded iframe.

    // Must be last.
    kServerCount
  };

  // Checked for no unconsumed events.
  std::vector<raw_ptr<WebContents, VectorExperimental>> event_sinks_;

  // Three servers to create three origins (different ports). One server for the
  // captured page, one for the top-level capturer and one for the embedded
  // capturer. Some tests will use one server for multiple pages so as to
  // make them same-origin.
  std::vector<std::unique_ptr<net::EmbeddedTestServer>> servers_;

  // Incognito browser.
  // Note: The regular one is accessible via browser().
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       HandleAndOriginExposedIfAllPermitted) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       HandleAndOriginExposedIfCapturerOriginPermitted) {
  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/false);

  TabInfo captured_tab = SetUpCapturedPage(/*expose_origin=*/true, "handle",
                                           {capturing_tab.GetOriginAsString()});

  capturing_tab.StartCapturing();

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       HandleAndOriginNotExposedIfCapturerOriginNotPermitted) {
  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/false);

  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {kArbitraryOrigin});

  capturing_tab.StartCapturing();

  // The capture handle isn't observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

// TODO(crbug.com/40185394): Test disabled on Mac due to multiple failing bots.
#if BUILDFLAG(IS_MAC)
#define MAYBE_HandleNotExposedIfTopLevelAllowlistedButCallingFrameNotAllowlisted \
  DISABLED_HandleNotExposedIfTopLevelAllowlistedButCallingFrameNotAllowlisted
#else
#define MAYBE_HandleNotExposedIfTopLevelAllowlistedButCallingFrameNotAllowlisted \
   HandleNotExposedIfTopLevelAllowlistedButCallingFrameNotAllowlisted
#endif

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    MAYBE_HandleNotExposedIfTopLevelAllowlistedButCallingFrameNotAllowlisted) {
  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/false);

  const url::Origin& top_level_capturer_origin =
      url::Origin::Create(servers_[kCapturingServer]->base_url());
  const url::Origin& embedded_capturer_origin =
      url::Origin::Create(servers_[kEmbeddedCapturingServer]->base_url());
  ASSERT_FALSE(
      top_level_capturer_origin.IsSameOriginWith(embedded_capturer_origin));

  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle",
                        {top_level_capturer_origin.Serialize()});

  capturing_tab.StartEmbeddingFrame(
      servers_[kEmbeddedCapturingServer]->GetURL(kCapturingPageEmbedded));
  capturing_tab.StartCapturingFromEmbeddedFrame();

  // The capture handle isn't observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleInEmbeddedFrame(),
            kNoEmbeddedCaptureHandle);

  // Even when the capture handle changes - no events are fired and the
  // capture handle remains unobservable via getCaptureHandle.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                                      {top_level_capturer_origin.Serialize()});
}

// TODO(crbug.com/40185394): Test disabled on Mac due to multiple failing bots.
// TODO(crbug.com/1287616, crbug.com/1362946): Flaky on Chrome OS and Windows.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
#define MAYBE_HandleExposedIfCallingFrameAllowlistedEvenIfTopLevelNotAllowlisted \
  DISABLED_HandleExposedIfCallingFrameAllowlistedEvenIfTopLevelNotAllowlisted
#else
#define MAYBE_HandleExposedIfCallingFrameAllowlistedEvenIfTopLevelNotAllowlisted \
   HandleExposedIfCallingFrameAllowlistedEvenIfTopLevelNotAllowlisted
#endif

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    MAYBE_HandleExposedIfCallingFrameAllowlistedEvenIfTopLevelNotAllowlisted) {
  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/false);

  const url::Origin& top_level_capturer_origin =
      url::Origin::Create(servers_[kCapturingServer]->base_url());
  const url::Origin& embedded_capturer_origin =
      url::Origin::Create(servers_[kEmbeddedCapturingServer]->base_url());
  ASSERT_FALSE(
      top_level_capturer_origin.IsSameOriginWith(embedded_capturer_origin));

  TabInfo captured_tab = SetUpCapturedPage(
      /*expose_origin=*/true, "handle", {embedded_capturer_origin.Serialize()});

  capturing_tab.StartEmbeddingFrame(
      servers_[kEmbeddedCapturingServer]->GetURL(kCapturingPageEmbedded));
  capturing_tab.StartCapturingFromEmbeddedFrame();

  // The capture handle is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleInEmbeddedFrame(),
            captured_tab.capture_handle);

  // When the capture handle changes, events are fired and the
  // capture handle remains observable via getCaptureHandle.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                                      {embedded_capturer_origin.Serialize()});
  EXPECT_EQ(capturing_tab.LastEmbeddedEvent(), captured_tab.capture_handle);
  EXPECT_EQ(capturing_tab.ReadCaptureHandleInEmbeddedFrame(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest, CanExposeOnlyHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/false, "handle", {"*"});
  ASSERT_EQ(captured_tab.capture_handle.find("origin"), std::string::npos);

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       CanExposeEmptyHandleIfExposingOrigin) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, /*handle=*/"", {"*"});
  // Still expecting "handle: \"\"" in there.
  ASSERT_NE(captured_tab.capture_handle.find("handle"), std::string::npos);

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       EmptyCaptureHandleConfigMeansCaptureHandleNotExposed) {
  // Note - even if we set permitted origins, the empty config is empty.
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/false, /*handle=*/"", {"*"});
  // Not expecting "handle: \"\"" in there, nor "origin:..."
  ASSERT_EQ(captured_tab.capture_handle, "");

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle isn't observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    CallingSetCaptureHandleConfigWithEmptyConfigFiresEventAndClearsValue) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getCaptureHandle produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/false, "", {});
  EXPECT_EQ(capturing_tab.LastEvent(), "null");
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    CallingSetCaptureHandleConfigWithNewHandleChangesConfigAndFiresEvent) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getCaptureHandle produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                                      {"*"});
  EXPECT_EQ(capturing_tab.LastEvent(), captured_tab.capture_handle);
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    CallingSetCaptureHandleConfigWithNewOriginValueChangesConfigAndFiresEvent) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getCaptureHandle produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/false, "handle", {"*"});
  EXPECT_EQ(capturing_tab.LastEvent(), captured_tab.capture_handle);
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    PermittedOriginsChangeThatRemovesCapturerCausesEventAndEmptyConfig) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getCaptureHandle produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "handle",
                                      {kArbitraryOrigin});
  EXPECT_EQ(capturing_tab.LastEvent(), "null");
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    PermittedOriginsChangeThatAddsCapturerCausesEventAndConfigExposure) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {kArbitraryOrigin});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getCaptureHandle produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "handle", {"*"});
  EXPECT_EQ(capturing_tab.LastEvent(), captured_tab.capture_handle);
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    PermittedOriginsChangeThatDoesNotAffectCapturerDoesNotCauseEventOrChange) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getCaptureHandle produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "handle",
                                      {capturing_tab.GetOriginAsString()});
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       SameDocumentNavigationDoesNotClearTheCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // Sanity test - there was an initial handle.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // In-document navigation does not change the capture handle (config).
  EXPECT_EQ(content::EvalJs(captured_tab.web_contents->GetPrimaryMainFrame(),
                            "clickLinkToPageBottom();"),
            "navigated");

  // No event was fired (verified in teardown) and getCaptureHandle returns the
  // same configuration as previously.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       CrossDocumentNavigationClearsTheCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // Sanity test - there was an initial handle.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // Cross-document navigation clears the capture handle (config).
  captured_tab.Navigate(servers_[kCapturedServer]->GetURL(kCapturedPageOther),
                        /*expect_handle_reset=*/true);

  // Navigation cleared the the capture handle, and that fired an event
  // with the empty CaptureHandle.
  EXPECT_EQ(capturing_tab.LastEvent(), "null");
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       CrossOriginNavigationClearsTheCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // Sanity test - there was an initial handle.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // Sanity over the test itself - the new server has a different origin.
  ASSERT_FALSE(url::Origin::Create(servers_[kOtherCapturedServer]->base_url())
                   .IsSameOriginWith(captured_tab.GetOrigin()));

  // Cross-origin navigation clears the capture handle (config) and fires
  // an event with the empty CaptureHandle.
  captured_tab.Navigate(
      servers_[kOtherCapturedServer]->GetURL(kCapturedPageOther),
      /*expect_handle_reset=*/true);
  EXPECT_EQ(capturing_tab.LastEvent(), "null");
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       SelfCaptureSanityWhenPermitted) {
  TabInfo tab = SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"},
                                  /*self_capture=*/true);
  tab.StartCapturing();

  // Correct initial value read.
  EXPECT_EQ(tab.ReadCaptureHandle(), tab.capture_handle);

  // Events correctly fired when self-capturing.
  tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle", {"*"});
  EXPECT_EQ(tab.LastEvent(), tab.capture_handle);
  EXPECT_EQ(tab.ReadCaptureHandle(), tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       SelfCaptureSanityWhenNotPermitted) {
  TabInfo tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {kArbitraryOrigin},
                        /*self_capture=*/true);

  ASSERT_TRUE(tab.GetOrigin().IsSameOriginWith(tab.GetOrigin()));

  tab.StartCapturing();

  // Correct initial value read.
  EXPECT_EQ(tab.ReadCaptureHandle(), "null");

  // No events fired when self-capturing but not allowed to observe.
  tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                             {kArbitraryOrigin});
  EXPECT_EQ(tab.ReadCaptureHandle(), "null");
}

// TODO(crbug.com/40772597): Disabled because of flakiness.
#if BUILDFLAG(IS_WIN)
#define MAYBE_RegularTabCannotReadIncognitoTabCaptureHandle \
  DISABLED_RegularTabCannotReadIncognitoTabCaptureHandle
#else
#define MAYBE_RegularTabCannotReadIncognitoTabCaptureHandle \
  RegularTabCannotReadIncognitoTabCaptureHandle
#endif
IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       MAYBE_RegularTabCannotReadIncognitoTabCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"},
                        /*self_capture=*/false, BrowserType::kIncognito);

  TabInfo capturing_tab =
      SetUpCapturingPage(/*start_capturing=*/true, BrowserType::kRegular);

  // Can neither observe the value when capture starts, nor receive events when
  // the capture handle changes.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                                      {"*"});
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

// TODO(crbug.com/40790671): Disabled because of flakiness.
IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       DISABLED_IncognitoTabCannotReadRegularTabCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"},
                        /*self_capture=*/false, BrowserType::kRegular);

  TabInfo capturing_tab =
      SetUpCapturingPage(/*start_capturing=*/true, BrowserType::kIncognito);

  // Can neither observe the value when capture starts, nor receive events when
  // the capture handle changes.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                                      {"*"});
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       IncognitoTabCannotReadIncognitoTabCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"},
                        /*self_capture=*/false, BrowserType::kIncognito);

  TabInfo capturing_tab =
      SetUpCapturingPage(/*start_capturing=*/true, BrowserType::kIncognito);

  // Can neither observe the value when capture starts, nor receive events when
  // the capture handle changes.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                                      {"*"});
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), "null");
}

// TODO(crbug.com/40772597): Disabled because of flakiness.
#if BUILDFLAG(IS_WIN)
#define MAYBE_IncognitoTabCanReadIncognitoTabCaptureHandleIfSelfCapture \
  DISABLED_IncognitoTabCanReadIncognitoTabCaptureHandleIfSelfCapture
#else
#define MAYBE_IncognitoTabCanReadIncognitoTabCaptureHandleIfSelfCapture \
  IncognitoTabCanReadIncognitoTabCaptureHandleIfSelfCapture
#endif
IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    MAYBE_IncognitoTabCanReadIncognitoTabCaptureHandleIfSelfCapture) {
  TabInfo tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"},
                        /*self_capture=*/true, BrowserType::kIncognito);

  tab.StartCapturing();

  // Can observe the value when capture starts.
  EXPECT_EQ(tab.ReadCaptureHandle(), tab.capture_handle);

  // Receives event of changes to the capture handle.
  tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle", {"*"});
  EXPECT_EQ(tab.LastEvent(), tab.capture_handle);
  EXPECT_EQ(tab.ReadCaptureHandle(), tab.capture_handle);
}

class CaptureHandleBrowserTestPrerender : public CaptureHandleBrowserTest {
 public:
  CaptureHandleBrowserTestPrerender() {
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(
            &CaptureHandleBrowserTestPrerender::GetCapturedWebContents,
            base::Unretained(this)));
  }

  content::WebContents* GetCapturedWebContents() {
    return captured_web_contents_;
  }

 protected:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> captured_web_contents_ =
      nullptr;
};

// Verifies that pre-rendered pages don't change the capture handle config.
IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTestPrerender,
                       EmptyConfigForCrossDocumentNavigations) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});
  captured_web_contents_ = captured_tab.web_contents;
  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);

  // Pre-render a document in the captured tab.
  const auto host_id = prerender_helper_->AddPrerender(
      servers_[kCapturedServer]->GetURL(kCapturedPageOther));
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_->GetPrerenderedMainFrameHost(host_id);
  EXPECT_EQ(content::EvalJs(prerender_rfh,
                            base::StringPrintf(
                                "callSetCaptureHandleConfig(%s, \"%s\", %s);",
                                "true", "prerender_handle",
                                StringifyPermittedOrigins({"*"}).c_str())),
            "capture-handle-set");
  EXPECT_EQ(capturing_tab.ReadCaptureHandle(), captured_tab.capture_handle);
}
