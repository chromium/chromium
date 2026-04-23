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
#include "chrome/test/interaction/tracked_element_webcontents.h"
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

// Caveat: This observer applies insets, so it will miss layout changes that
// only affect the border/insets but not the content bounds.
class ViewContentBoundsObserver : public views::ViewObserver,
                                  public ui::test::StateObserver<gfx::Rect> {
 public:
  explicit ViewContentBoundsObserver(const raw_ptr<views::View>& view)
      : view_(view) {
    observation_.Observe(view);
  }

  // ui::test::StateObserver:
  gfx::Rect GetStateObserverInitialState() const override {
    return GetContentBounds();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override {
    OnStateObserverStateChanged(GetContentBounds());
  }
  void OnViewIsDeleting(views::View* view) override {
    view_ = nullptr;
    observation_.Reset();
  }

 private:
  gfx::Rect GetContentBounds() const {
    gfx::Rect bounds = view_->GetBoundsInScreen();
    bounds.Inset(view_->GetInsets());
    return bounds;
  }

  raw_ptr<views::View> view_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ViewContentBoundsObserver,
                                    kToolbarBoundsState);

MATCHER_P(IsCloseToTopRightOf, image_bounds_ref, "") {
  const gfx::Rect& image_bounds = image_bounds_ref.get();
  return std::abs(arg.right() - image_bounds.right()) <= 30 &&
         arg.right() <= image_bounds.right() &&
         std::abs(arg.y() - image_bounds.y()) <= 30 &&
         arg.y() >= image_bounds.y();
}

class IndigoBrowserTest : public InteractiveBrowserTest {
 public:
  IndigoBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kIndigo, blink::features::kImageReplacement}, {});
  }
  ~IndigoBrowserTest() override = default;

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

IN_PROC_BROWSER_TEST_F(IndigoBrowserTest, ToolbarPositioning) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 50, 512, 512};

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      PressButton(kIndigoPageActionIconElementId),

      AfterShow(IndigoToolbar::kToolbarElementId,
                base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                  toolbar_view = AsView(el);
                })),
      ObserveState(kToolbarBoundsState, std::ref(toolbar_view)),

      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),

      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))));
}

class IndigoHighDsfBrowserTest : public IndigoBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IndigoBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");
  }
};

IN_PROC_BROWSER_TEST_F(IndigoHighDsfBrowserTest, ToolbarPositioning) {
  const GURL url = embedded_test_server()->GetURL("/image.html");
  raw_ptr<views::View> toolbar_view = nullptr;
  gfx::Rect image_bounds{50, 50, 512, 512};

  RunTestSequence(
      InstrumentTab(kWebContentsId), NavigateWebContents(kWebContentsId, url),
      WaitForShow(kIndigoPageActionIconElementId),
      PressButton(kIndigoPageActionIconElementId),

      AfterShow(IndigoToolbar::kToolbarElementId,
                base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                  toolbar_view = AsView(el);
                })),
      ObserveState(kToolbarBoundsState, std::ref(toolbar_view)),

      WithElement(kWebContentsId,
                  base::BindLambdaForTesting([&](ui::TrackedElement* el) {
                    auto* contents = el->AsA<TrackedElementWebContents>();
                    views::WebView* web_view = contents->owner()->GetWebView();
                    views::View::ConvertRectToScreen(web_view, &image_bounds);
                  })),

      WaitForState(kToolbarBoundsState,
                   IsCloseToTopRightOf(std::ref(image_bounds))));
}

}  // namespace
}  // namespace indigo
