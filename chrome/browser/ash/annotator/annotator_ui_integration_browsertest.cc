// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/annotator/annotator_client_impl.h"
#include "ash/webui/annotator/public/cpp/annotator_client.h"
#include "ash/webui/media_app_ui/test/media_app_ui_browsertest.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/files/safe_base_name.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

// File containing the test utility library
constexpr base::FilePath::CharType kTestLibraryPath[] =
    FILE_PATH_LITERAL("ash/webui/system_apps/public/js/dom_testing_helpers.js");

content::EvalJsResult EvalJsInMainFrame(content::WebContents* web_ui,
                                        const std::string& script) {
  // Clients of this helper all run in the same isolated world.
  constexpr int kWorldId = 1;
  return EvalJs(web_ui->GetPrimaryMainFrame(), script,
                content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, kWorldId);
}

void PrepareAnnotatorForTest(content::WebContents* web_contents) {
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_EQ(nullptr,
            EvalJsInMainFrame(web_contents,
                              SandboxedWebUiAppTestBase::LoadJsTestLibrary(
                                  base::FilePath(kTestLibraryPath))));
}
}  // namespace

class AnnotatorUITest : public InProcessBrowserTest {
 public:
  void LoadAnnotatorUI() {
    // Load the annotator UI.
    VerifyUrlValid(ash::kChromeUIUntrustedAnnotatorUrl);
    AnnotatorClientImpl* annotator_client =
        static_cast<AnnotatorClientImpl*>(ash::AnnotatorClient::Get());
    annotator_embedder_ = annotator_client->get_annotator_handler_for_test()
                              ->get_web_ui_for_test()
                              ->GetWebContents();
  }
  // This test helper verifies that navigating to the |url| doesn't result in a
  // 404 error.
  void VerifyUrlValid(const char* url) {
    GURL gurl(url);
    EXPECT_TRUE(gurl.is_valid()) << "url isn't valid: " << url;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl))
        << "navigating to url failed: " << url;
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
              content::PAGE_TYPE_NORMAL)
        << "page has unexpected errors: " << url;
  }

  content::WebContents* annotator_embedder() { return annotator_embedder_; }

 private:
  raw_ptr<content::WebContents> annotator_embedder_ = nullptr;
};

// These tests try to load files bundled in our CIPD package. The CIPD package
// is included in the `linux-chromeos-chrome` trybot but not in
// `linux-chromeos-rel` trybot. Our CIPD package is only present when both the
// media app and Projector are enabled. We disable the tests rather than comment
// them out entirely so that they are still subject to compilation on
// open-source builds.
// For now annotator is available only when the Projector app is enabled. In the
// future, the annotator will be a standalone feature.
// TODO(crbug.com/345725094): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(AnnotatorUITest, DISABLED_LoadsInkForAnnotator) {
  LoadAnnotatorUI();
  PrepareAnnotatorForTest(annotator_embedder());
  // Checks ink is loaded by ensuring the ink engine canvas has a non zero width
  // and height attributes (checking <canvas.width/height is insufficient since
  // it has a default width of 300 and height of 150). Note: The loading of ink
  // engine elements can be async.
  constexpr char kCheckInkLoaded[] = R"(
      (async function checkInkLoaded() {
        const inkCanvas = await getNode('canvas',
          ['projector-ink-canvas-wrapper']);
        return !!inkCanvas &&
          !!inkCanvas.getAttribute('height') &&
          inkCanvas.getAttribute('height') !== '0' &&
          !!inkCanvas.getAttribute('width') &&
          inkCanvas.getAttribute('width') !== '0';
      })();
    )";
  EXPECT_EQ(
      true,
      EvalJsInMainFrame(annotator_embedder(), kCheckInkLoaded).ExtractBool());
}
}  // namespace ash
