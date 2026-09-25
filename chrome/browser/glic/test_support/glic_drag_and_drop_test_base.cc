// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_drag_and_drop_test_base.h"

#include "base/files/file_util.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/gfx/geometry/rect.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/ui_test_utils.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#endif

namespace glic {

GlicDragAndDropTestBase::GlicDragAndDropTestBase(GlicTestJsPath js_test_file,
                                                 bool enable_no_webview)
    : GlicApiBrowserTest(js_test_file), is_no_webview_(enable_no_webview) {
  std::vector<base::test::FeatureRef> enabled_features = {
      features::kGlicDragAndDropFileUpload,
      features::kGlicWebDragAndDropFileUpload};
  std::vector<base::test::FeatureRef> disabled_features;

  if (enable_no_webview) {
    enabled_features.push_back(features::kGlicNoWebview);
  } else {
    disabled_features.push_back(features::kGlicNoWebview);
  }

  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

GlicDragAndDropTestBase::~GlicDragAndDropTestBase() = default;

void GlicDragAndDropTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  GlicApiBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(::switches::kGlicDev);
  // Skips FRE experience.
  command_line->AppendSwitch(::switches::kGlicAutomation);
}

void GlicDragAndDropTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");

  GlicApiBrowserTest::SetUpOnMainThread();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  signin::MakePrimaryAccountAvailable(identity_manager, "foo@google.com",
                                      signin::ConsentLevel::kSignin);
  signin::SetRefreshTokenForPrimaryAccount(identity_manager);
}

void GlicDragAndDropTestBase::TearDownOnMainThread() {
  GlicApiBrowserTest::TearDownOnMainThread();
}

void GlicDragAndDropTestBase::PrepareGuestForDrag(Host& glic_host) {
  content::WebContents* guest_contents = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    guest_contents = glic_host.web_client_contents();
    return guest_contents != nullptr;
  }));
  EXPECT_TRUE(content::WaitForLoadStop(guest_contents));
  ExecuteJsTest();

  content::RenderWidgetHost* rwh =
      glic_host.GetGuestMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !rwh->GetView()->GetViewBounds().IsEmpty() &&
           !glic_host.webui_contents()
                ->GetRenderWidgetHostView()
                ->GetViewBounds()
                .IsEmpty();
  }));
  // Ensure hit test data is ready for the guest.
  content::WaitForHitTestData(glic_host.GetGuestMainFrame());
}

gfx::Point GlicDragAndDropTestBase::GetGuestCenterInHost(Host& glic_host) {
  auto* guest_view =
      glic_host.GetGuestMainFrame()->GetRenderWidgetHost()->GetView();
  auto* host_view = glic_host.webui_contents()->GetRenderWidgetHostView();

  gfx::Rect guest_bounds = guest_view->GetViewBounds();
  gfx::Rect host_bounds = host_view->GetViewBounds();

  return guest_bounds.CenterPoint() - host_bounds.OffsetFromOrigin();
}

base::FilePath GlicDragAndDropTestBase::CreateTestFile(
    base::ScopedTempDir& temp_dir,
    const std::string& name,
    const std::string& contents) {
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath test_file = temp_dir.GetPath().AppendASCII(name);
  base::WriteFile(test_file, base::as_byte_span(contents));
  return test_file;
}

#if !BUILDFLAG(IS_ANDROID)
content::WebContents*
GlicDragAndDropTestBase::SetupSourceTabWithDraggableImage() {
  if (!ui_test_utils::NavigateToURL(
          GetBrowser(), embedded_test_server()->GetURL(
                            "a.com", "/drag_and_drop/image_source.html"))) {
    ADD_FAILURE() << "NavigateToURL failed";
    return nullptr;
  }
  content::WebContents* source_wc =
      GetBrowser()->tab_strip_model()->GetActiveWebContents();
  if (!source_wc) {
    ADD_FAILURE() << "No active web contents";
    return nullptr;
  }
  source_wc->Focus();

  // Fix the image src so it's draggable.
  GURL img_url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/cors-allowed.jpg");
  if (!content::ExecJs(source_wc, content::JsReplace(
                                      "document.querySelector('img').src = $1",
                                      img_url.spec()))) {
    ADD_FAILURE() << "ExecJs to set image src failed";
    return nullptr;
  }

  // Wait for the image to load and lay out before querying its bounds.
  auto result = content::EvalJs(
      source_wc,
      "const img = document.querySelector('img');"
      "new Promise(resolve => {"
      "  if (img.complete && img.naturalWidth > 0) { resolve(true); }"
      "  else { img.onload = () => resolve(true); img.onerror = () => "
      "resolve(false); }"
      "})");
  if (!result.is_ok() || !result.ExtractBool()) {
    ADD_FAILURE() << "Image did not load successfully";
    return nullptr;
  }
  return source_wc;
}

bool GlicDragAndDropTestBase::SimulateMouseDownAndWait(
    content::WebContents* source_wc,
    const gfx::Point& point) {
  if (!content::ExecJs(
          source_wc,
          "window.__mouseDownReceived = false;"
          "window.addEventListener('mousedown', () => "
          "{ window.__mouseDownReceived = true; }, {once: true});")) {
    return false;
  }

  content::SimulateMouseEvent(source_wc, blink::WebInputEvent::Type::kMouseDown,
                              blink::WebMouseEvent::Button::kLeft, point);

  return base::test::RunUntil([&]() {
    return content::EvalJs(source_wc, "window.__mouseDownReceived")
        .ExtractBool();
  });
}

void GlicDragAndDropTestBase::SimulateMouseDragFromImage(
    content::WebContents* source_wc) {
  // We use Javascript to find the image center and then click+drag it.
  double img_x = content::EvalJs(source_wc,
                                 "const img = document.querySelector('img');"
                                 "const rect = img.getBoundingClientRect();"
                                 "rect.left + rect.width / 2")
                     .ExtractDouble();
  double img_y = content::EvalJs(source_wc,
                                 "const img = document.querySelector('img');"
                                 "const rect = img.getBoundingClientRect();"
                                 "rect.top + rect.height / 2")
                     .ExtractDouble();

  gfx::Point drag_start_point(img_x, img_y);
  gfx::Point drag_end_point = drag_start_point + gfx::Vector2d(100, 100);

  ASSERT_TRUE(SimulateMouseDownAndWait(source_wc, drag_start_point));

  content::SimulateMouseEvent(source_wc, blink::WebInputEvent::Type::kMouseMove,
                              blink::WebMouseEvent::Button::kLeft,
                              drag_start_point + gfx::Vector2d(20, 20));
  content::SimulateMouseEvent(source_wc, blink::WebInputEvent::Type::kMouseMove,
                              blink::WebMouseEvent::Button::kLeft,
                              drag_start_point + gfx::Vector2d(40, 40));
  content::SimulateMouseEvent(source_wc, blink::WebInputEvent::Type::kMouseMove,
                              blink::WebMouseEvent::Button::kLeft,
                              drag_end_point);
}
#endif

}  // namespace glic
