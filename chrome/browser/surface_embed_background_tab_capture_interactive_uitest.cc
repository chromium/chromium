// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/surface_embed/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {
constexpr char kCapturedPageTitle[] = "Background Captured Page";
}  // namespace

class SurfaceEmbedBackgroundTabCaptureInteractiveUiTest
    : public WebRtcTestBase {
 public:
  SurfaceEmbedBackgroundTabCaptureInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {surface_embed::features::kSurfaceEmbed, ::features::kWebium},
        {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebRtcTestBase::SetUpInProcessBrowserTestFixture();
    DetectErrorsInJavaScript();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoSelectTabCaptureSourceByTitle, kCapturedPageTitle);
  }

  content::WebContents* OpenTestPageInNewTab(const std::string& test_url) {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
    GURL url = embedded_test_server()->GetURL(test_url);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER))
// TODO(crbug.com/451876195): Enable this test for CrOS once WebUIBrowser
// window management is fixed on CrOS.
// TODO(crbug.com/515107079): Flaky on Linux MSan.
#define MAYBE_KeepAliveWorksForCapturedBackgroundTab \
  DISABLED_KeepAliveWorksForCapturedBackgroundTab
#else
#define MAYBE_KeepAliveWorksForCapturedBackgroundTab \
  KeepAliveWorksForCapturedBackgroundTab
#endif

IN_PROC_BROWSER_TEST_F(SurfaceEmbedBackgroundTabCaptureInteractiveUiTest,
                       MAYBE_KeepAliveWorksForCapturedBackgroundTab) {
  // Open the captured tab first. It will be the background tab later.
  content::WebContents* captured_tab =
      OpenTestPageInNewTab("/surface_embed/background_captured_page.html");

  // Open the capturing tab. It becomes the foreground active tab.
  content::WebContents* capturing_tab =
      OpenTestPageInNewTab("/webrtc/webrtc_getdisplaymedia_test.html");

  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            capturing_tab);
  EXPECT_NE(captured_tab, capturing_tab);

  // Trigger getDisplayMedia() in the foreground tab. Note that the media picker
  // prompt is automatically bypassed and the target tab is selected due to the
  // kAutoSelectTabCaptureSourceByTitle switch configured in SetUpCommandLine.
  // We use the runGetDisplayMedia() function defined in
  // webrtc_getdisplaymedia_test.html.
  std::string script = R"(
    navigator.mediaDevices.getDisplayMedia({video: true})
      .then(stream => {
        window.stream = stream;
        return 'capture-success';
      })
      .catch(e => e.toString());
  )";

  EXPECT_EQ("capture-success", content::EvalJs(capturing_tab, script));

  // Now, the background tab should be captured. Its keep-alive should be
  // active. If keep-alive is working, requestAnimationFrame will keep ticking.
  // We wait for window.tickCount to reach 10.
  std::string background_script = R"(
    new Promise(resolve => {
      function check() {
        if (window.tickCount >= 10) {
          resolve('ticks-ok');
        } else {
          requestAnimationFrame(check);
        }
      }
      check();
    });
  )";

  EXPECT_EQ("ticks-ok", content::EvalJs(captured_tab, background_script));
}
