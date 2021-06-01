// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "build/buildflag.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"

// TODO(crbug.com/1215089): Enable this test suite on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

using content::WebContents;

namespace {

// The captured tab is identified by its title.
const char kCapturedTabTitle[] = "totally-unique-captured-page-title";

// Capturing page.
const char kCapturingPageMain[] = "/webrtc/capturing_page_main.html";
// Captured page.
const char kCapturedPageMain[] = "/webrtc/captured_page_main.html";
// Similar contents to kCapturedPageMain, but on a different page, which can
// be served same-origin or cross-origin.
const char kCapturedPageOther[] = "/webrtc/captured_page_other.html";

const char* kArbitraryOrigin = "https://arbitrary-origin.com";
const char* kNoCaptureHandle = "no-capture-handle";

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
        url::Origin::Create(web_contents->GetLastCommittedURL());
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
    std::string script_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(), "captureOtherTab();", &script_result));
    EXPECT_EQ(script_result, "capture-success");
  }

  url::Origin GetOrigin() const {
    return url::Origin::Create(web_contents->GetLastCommittedURL());
  }

  std::string GetOriginAsString() const { return GetOrigin().Serialize(); }

  void SetCaptureHandleConfig(
      bool expose_origin,
      const std::string& handle,
      const std::vector<std::string>& permitted_origins) {
    std::string script_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(),
        base::StringPrintf(
            "callSetCaptureHandleConfig(%s, \"%s\", %s);",
            expose_origin ? "true" : "false", handle.c_str(),
            StringifyPermittedOrigins(permitted_origins).c_str()),
        &script_result));
    EXPECT_EQ(script_result, "capture-handle-set");

    capture_handle =
        StringifyCaptureHandle(web_contents, expose_origin, handle);
  }

  std::string ReadCaptureHandleFromSettings() {
    std::string script_result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(), "readCaptureHandleFromSettings();",
        &script_result));
    return script_result;
  }

  void Navigate(Browser* browser, GURL url, bool expect_handle_reset = false) {
    std::string script_result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(),
        base::StringPrintf("clickLinkToUrl(\"%s\");", url.spec().c_str()),
        &script_result));
    ASSERT_EQ(script_result, "link-success");

    if (expect_handle_reset) {
      capture_handle = "";
    }
  }

  std::string LastEvent() {
    std::string script_result = "error-not-modified";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents->GetMainFrame(), "readLastEvent();", &script_result));
    return script_result;
  }

  WebContents* web_contents;
  std::string capture_handle;  // Expected value for those who may observe.
};

TabInfo MakeTabInfo(WebContents* web_contents,
                    bool expose_origin,
                    const std::string& handle) {
  return TabInfo{web_contents,
                 StringifyCaptureHandle(web_contents, expose_origin, handle)};
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

    for (size_t i = 0; i < 3; ++i) {
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
  WebContents* OpenTestPageInNewTab(const std::string& test_page,
                                    net::EmbeddedTestServer* server) const {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
    GURL url = server->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
    WebContents* new_tab = browser()->tab_strip_model()->GetActiveWebContents();
    permissions::PermissionRequestManager::FromWebContents(new_tab)
        ->set_auto_response_for_test(
            permissions::PermissionRequestManager::ACCEPT_ALL);
    return new_tab;
  }

  TabInfo SetUpCapturingPage(bool start_capturing) {
    auto* const web_contents = OpenTestPageInNewTab(
        kCapturingPageMain, servers_[kCapturingServer].get());

    auto result = MakeTabInfo(web_contents, true, "capturing_page");
    if (start_capturing) {
      result.StartCapturing();
    }

    event_sinks_.push_back(web_contents);

    return result;
  }

  TabInfo SetUpCapturedPage(bool expose_origin,
                            const std::string& handle,
                            const std::vector<std::string>& permitted_origins,
                            bool self_capture = false) {
    // Normally, the captured page has its own server (=origin) and own file.
    // But if self-capture is tested, use the origin and page of the capturer.
    const char* page = self_capture ? kCapturingPageMain : kCapturedPageMain;
    const int server_index = self_capture ? kCapturingServer : kCapturedServer;

    auto* const web_contents =
        OpenTestPageInNewTab(page, servers_[server_index].get());

    // The target for getDisplayMedia is determined via the title. If we want
    // the capturing page to capture itself, then it has to change its title.
    if (self_capture) {
      std::string script_result;
      EXPECT_TRUE(content::ExecuteScriptAndExtractString(
          web_contents->GetMainFrame(),
          base::StringPrintf("setTitle(\"%s\");", kCapturedTabTitle),
          &script_result));
      EXPECT_EQ(script_result, "title-changed");
    }

    auto tab_info = MakeTabInfo(web_contents, expose_origin, handle);

    tab_info.SetCaptureHandleConfig(expose_origin, handle, permitted_origins);

    return tab_info;
  }

  static constexpr size_t kCapturedServer = 0;
  static constexpr size_t kCapturingServer = 1;
  static constexpr size_t kOtherCapturedServer = 2;

  // Checked for no unconsumed events.
  std::vector<WebContents*> event_sinks_;

  // Three servers to create three origins (different ports). One server for the
  // captured page, one for the top-level capturer and one for the embedded
  // capturer. Some tests will use one server for multiple pages so as to
  // make them same-origin.
  std::vector<std::unique_ptr<net::EmbeddedTestServer>> servers_;
};

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       HandleAndOriginExposedIfAllPermitted) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       HandleAndOriginExposedIfCapturerOriginPermitted) {
  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/false);

  TabInfo captured_tab = SetUpCapturedPage(/*expose_origin=*/true, "handle",
                                           {capturing_tab.GetOriginAsString()});

  capturing_tab.StartCapturing();

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       HandleAndOriginNotExposedIfCapturerOriginNotPermitted) {
  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/false);

  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {kArbitraryOrigin});

  capturing_tab.StartCapturing();

  // The capture handle isn't observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest, CanExposeOnlyHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/false, "handle", {"*"});
  ASSERT_EQ(captured_tab.capture_handle.find("origin"), std::string::npos);

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       CanExposeEmptyHandleIfExposingOrigin) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, /*handle=*/"", {"*"});
  // Still expecting "handle: \"\"" in there.
  ASSERT_NE(captured_tab.capture_handle.find("handle"), std::string::npos);

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
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
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    CallingSetCaptureHandleConfigWithEmptyConfigFiresEventAndClearsValue) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getSettings produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/false, "", {});
  EXPECT_EQ(capturing_tab.LastEvent(), "{}");
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    CallingSetCaptureHandleConfigWithNewHandleChangesConfigAndFiresEvent) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getSettings produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                                      {"*"});
  EXPECT_EQ(capturing_tab.LastEvent(), captured_tab.capture_handle);
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    CallingSetCaptureHandleConfigWithNewOriginValueChangesConfigAndFiresEvent) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getSettings produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/false, "handle", {"*"});
  EXPECT_EQ(capturing_tab.LastEvent(), captured_tab.capture_handle);
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    PermittedOriginsChangeThatRemovesCapturerCausesEventAndEmptyConfig) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getSettings produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "handle",
                                      {kArbitraryOrigin});
  EXPECT_EQ(capturing_tab.LastEvent(), "{}");
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    PermittedOriginsChangeThatAddsCapturerCausesEventAndConfigExposure) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {kArbitraryOrigin});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getSettings produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "handle", {"*"});
  EXPECT_EQ(capturing_tab.LastEvent(), captured_tab.capture_handle);
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(
    CaptureHandleBrowserTest,
    PermittedOriginsChangeThatDoesNotAffectCapturerDoesNotCauseEventOrChange) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // The capture handle set by the captured tab is observable by the capturer.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);

  // New CaptureHandleConfig set by captured tab triggers an event, and all
  // subsequent calls to getSettings produce the new values.
  captured_tab.SetCaptureHandleConfig(/*expose_origin=*/true, "handle",
                                      {capturing_tab.GetOriginAsString()});
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       SameDocumentNavigationDoesNotClearTheCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // Sanity test - there was an initial handle.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);

  // In-document navigation does not change the capture handle (config).
  std::string navigation_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      captured_tab.web_contents->GetMainFrame(), "clickLinkToPageBottom();",
      &navigation_result));
  ASSERT_EQ(navigation_result, "navigated");

  // No event was fired (verified in teardown) and getSettings returns the
  // same configuration as previously.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       CrossDocumentNavigationClearsTheCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // Sanity test - there was an initial handle.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);

  // Cross-document navigation clears the capture handle (config).
  captured_tab.Navigate(browser(),
                        servers_[kCapturedServer]->GetURL(kCapturedPageOther),
                        /*expect_handle_reset=*/true);

  // Navigation cleared the the capture handle, and that fired an event
  // with the empty CaptureHandle.
  EXPECT_EQ(capturing_tab.LastEvent(), "{}");
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       CrossOriginNavigationClearsTheCaptureHandle) {
  TabInfo captured_tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"});

  TabInfo capturing_tab = SetUpCapturingPage(/*start_capturing=*/true);

  // Sanity test - there was an initial handle.
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(),
            captured_tab.capture_handle);

  // Sanity over the test itself - the new server has a different origin.
  ASSERT_FALSE(url::Origin::Create(servers_[kOtherCapturedServer]->base_url())
                   .IsSameOriginWith(captured_tab.GetOrigin()));

  // Cross-origin navigation clears the capture handle (config) and fires
  // an event with the empty CaptureHandle.
  captured_tab.Navigate(
      browser(), servers_[kOtherCapturedServer]->GetURL(kCapturedPageOther),
      /*expect_handle_reset=*/true);
  EXPECT_EQ(capturing_tab.LastEvent(), "{}");
  EXPECT_EQ(capturing_tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       SelfCaptureSanityWhenPermitted) {
  TabInfo tab = SetUpCapturedPage(/*expose_origin=*/true, "handle", {"*"},
                                  /*self_capture=*/true);
  tab.StartCapturing();

  // Correct initial value read.
  EXPECT_EQ(tab.ReadCaptureHandleFromSettings(), tab.capture_handle);

  // Events correctly fired when self-capturing.
  tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle", {"*"});
  EXPECT_EQ(tab.LastEvent(), tab.capture_handle);
  EXPECT_EQ(tab.ReadCaptureHandleFromSettings(), tab.capture_handle);
}

IN_PROC_BROWSER_TEST_F(CaptureHandleBrowserTest,
                       SelfCaptureSanityWhenNotPermitted) {
  TabInfo tab =
      SetUpCapturedPage(/*expose_origin=*/true, "handle", {kArbitraryOrigin},
                        /*self_capture=*/true);

  ASSERT_TRUE(tab.GetOrigin().IsSameOriginWith(tab.GetOrigin()));

  tab.StartCapturing();

  // Correct initial value read.
  EXPECT_EQ(tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);

  // No events fired when self-capturing but not allowed to observe..
  tab.SetCaptureHandleConfig(/*expose_origin=*/true, "new_handle",
                             {kArbitraryOrigin});
  EXPECT_EQ(tab.ReadCaptureHandleFromSettings(), kNoCaptureHandle);
}

#endif  //  !BUILDFLAG(IS_CHROMEOS_LACROS)
