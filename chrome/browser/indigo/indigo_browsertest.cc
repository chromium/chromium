// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/display_switches.h"

namespace indigo {
namespace {

const char kStubScript[] = R"(
  const agent = {
    invoke: function() {
      const img = document.getElementById('target_image');
      if (img) {
        window.indigo.startImageReplacement(img);
      } else {
        console.error('Target image not found');
      }
    }
  };
  window.indigo.setup(agent);
)";

const char kHtmlBody[] = R"(
<!DOCTYPE html>
<html><body>
<img id="target_image"
     src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg=="
     style="width:512px; height:512px; position:absolute; left:50px; top:50px;">
</body></html>)";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);

class IndigoBrowserTest : public InteractiveBrowserTest {
 public:
  IndigoBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIndigo, blink::features::kImageReplacement}, {});
  }
  ~IndigoBrowserTest() override = default;

  gfx::Rect GetContainerBounds() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetContainerBounds();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    // Force Indigo page action and point to the generated stub script.
    command_line->AppendSwitch("force-indigo");
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath script_path =
        temp_dir_.GetPath().AppendASCII("stub_script.js");
    CHECK(base::WriteFile(script_path, kStubScript));
    command_line->AppendSwitchASCII("indigo-script",
                                    script_path.AsUTF8Unsafe());
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == "/image.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content(kHtmlBody);
            response->set_content_type("text/html");
            return response;
          }
          return nullptr;
        }));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

// TODO(crbug.com/504741803): Re-enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ToolbarPositioning DISABLED_ToolbarPositioning
#else
#define MAYBE_ToolbarPositioning ToolbarPositioning
#endif
IN_PROC_BROWSER_TEST_F(IndigoBrowserTest, MAYBE_ToolbarPositioning) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  gfx::Rect toolbar_bounds;
  gfx::Rect image_bounds;

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      PressButton(kIndigoPageActionIconElementId),
      WaitForShow(IndigoToolbar::kToolbarElementId),

      Do(base::BindLambdaForTesting([&]() {
        gfx::Rect container_bounds = GetContainerBounds();
        image_bounds = gfx::Rect(container_bounds.x() + 50,
                                 container_bounds.y() + 50, 512, 512);
      })),

      WithView(IndigoToolbar::kToolbarElementId,
               base::BindLambdaForTesting([&](views::View* view) {
                 toolbar_bounds = view->GetBoundsInScreen();
                 toolbar_bounds.Inset(view->GetInsets());
               })),

      Do(base::BindLambdaForTesting([&]() {
        EXPECT_NEAR(image_bounds.right(), toolbar_bounds.right(), 30);
        EXPECT_GE(image_bounds.right(), toolbar_bounds.right());
        EXPECT_NEAR(image_bounds.y(), toolbar_bounds.y(), 30);
        EXPECT_LE(image_bounds.y(), toolbar_bounds.y());
      })));
}

class IndigoHighDsfBrowserTest : public IndigoBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IndigoBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

IN_PROC_BROWSER_TEST_F(IndigoHighDsfBrowserTest, MAYBE_ToolbarPositioning) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  gfx::Rect toolbar_bounds;
  gfx::Rect image_bounds;

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      PressButton(kIndigoPageActionIconElementId),
      WaitForShow(IndigoToolbar::kToolbarElementId),

      Do(base::BindLambdaForTesting([&]() {
        gfx::Rect container_bounds = GetContainerBounds();
        image_bounds = gfx::Rect(container_bounds.x() + 50,
                                 container_bounds.y() + 50, 512, 512);
      })),

      WithView(IndigoToolbar::kToolbarElementId,
               base::BindLambdaForTesting([&](views::View* view) {
                 toolbar_bounds = view->GetBoundsInScreen();
                 toolbar_bounds.Inset(view->GetInsets());
               })),

      Do(base::BindLambdaForTesting([&]() {
        EXPECT_NEAR(image_bounds.right(), toolbar_bounds.right(), 30);
        EXPECT_GE(image_bounds.right(), toolbar_bounds.right());
        EXPECT_NEAR(image_bounds.y(), toolbar_bounds.y(), 30);
        EXPECT_LE(image_bounds.y(), toolbar_bounds.y());
      })));
}

}  // namespace
}  // namespace indigo
