// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_drag_and_drop_test_base.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

#if BUILDFLAG(IS_WIN)
#include "ui/events/platform/platform_event_source.h"
#endif

namespace glic {
namespace {

std::optional<gfx::Point> GetAlignedGuestCenterInHost(Host& glic_host,
                                                      bool is_no_webview) {
  content::RenderFrameHost* guest_rfh = glic_host.GetGuestMainFrame();
  content::RenderWidgetHostView* guest_view =
      guest_rfh ? guest_rfh->GetRenderWidgetHost()->GetView() : nullptr;
  if (!guest_view) {
    return std::nullopt;
  }

  const gfx::Rect guest_bounds = guest_view->GetViewBounds();
  if (guest_bounds.width() <= 1 || guest_bounds.height() <= 1) {
    return std::nullopt;
  }

  // In `NoWebview` mode, `glic_host.webui_contents()` is the guest
  // `WebContents` itself, so the guest's local center is already in host
  // coordinates.
  const gfx::Point guest_local_center(guest_bounds.width() / 2,
                                      guest_bounds.height() / 2);
  if (is_no_webview) {
    return guest_local_center;
  }

  content::RenderWidgetHostView* host_view =
      glic_host.webui_contents()->GetRenderWidgetHostView();
  if (!host_view) {
    return std::nullopt;
  }

  const gfx::Rect host_bounds = host_view->GetViewBounds();
  // While `#guestPanel` is hidden (`display: none`), `GuestViewContainer`
  // initializes `<webview class="guest-frame">` (`height: 100%`) to 300x100.
  // Wait until `<webview>` resizes to fill the visible host panel.
  if (host_bounds.IsEmpty() ||
      (host_bounds.height() > 200 && guest_bounds.height() <= 100)) {
    return std::nullopt;
  }

  // `WaitForHitTestData()` returns as soon as the guest's `FrameSinkId` is
  // present in Viz's `HitTestQuery`, which can already be true from the
  // initial 300x100 hidden surface before Viz aggregates the resized
  // `<webview>` transform. Verify that Viz transforms the guest's local
  // center to the expected host-relative point within
  // `RenderWidgetTargeter::FoundFrameSinkId`'s 2.0 DIP tolerance.
  const gfx::Point host_center =
      guest_bounds.CenterPoint() - host_bounds.OffsetFromOrigin();
  const gfx::PointF viz_host_center =
      guest_view->TransformPointToRootCoordSpaceF(
          gfx::PointF(guest_local_center));
  if (!viz_host_center.IsWithinDistance(gfx::PointF(host_center), 2.0f)) {
    return std::nullopt;
  }
  return host_center;
}

}  // namespace

GlicDragAndDropTestBase::GlicDragAndDropTestBase(GlicTestJsPath js_test_file,
                                                 bool enable_no_webview)
    : GlicApiBrowserTest(js_test_file), is_no_webview_(enable_no_webview) {
  std::vector<base::test::FeatureRef> enabled_features = {
      features::kGlicDragAndDropFileUpload,
      features::kGlicDragAndDropFileUploadAndroid,
      features::kGlicWebDragAndDropFileUpload,
  };
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
#if BUILDFLAG(IS_WIN)
  old_ignore_native_platform_events_ =
      ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents();
  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);
#endif

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
#if BUILDFLAG(IS_WIN)
  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(
      old_ignore_native_platform_events_);
#endif
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

  // Wait for `chrome://glic` to transition out of `kHoldLoading` into `kReady`
  // (or, under `kGlicNoWebview`, for `GlicNoWebviewContentsManager` to swap
  // `active_web_contents()` to `guest_contents`).
  if (is_no_webview_) {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return glic_host.webui_contents() == guest_contents; }));
  } else {
    ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kReady));
  }
#if BUILDFLAG(IS_ANDROID)
  // Disable and blur the autofocus `<input id="inputBox">` in `test.html` so
  // Android's `InputMethodManager` does not keep the soft keyboard open and
  // resize the viewport mid-drag.
  constexpr char kBlurGuestInputScript[] = R"js(
    const input = document.getElementById('inputBox');
    if (input) { input.disabled = true; input.blur(); }
    if (document.activeElement instanceof HTMLElement) {
      document.activeElement.blur();
    }
  )js";
  ASSERT_TRUE(
      content::ExecJs(glic_host.GetGuestMainFrame(), kBlurGuestInputScript));
#endif  // BUILDFLAG(IS_ANDROID)

  content::RenderWidgetHost* rwh =
      glic_host.GetGuestMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  if (!is_no_webview_) {
    // Allow `showPanel('guestPanel')` to finish un-hiding and laying out the
    // `<webview>` container before sampling its view bounds.
    constexpr char kWaitForTwoFramesScript[] = R"js(
      new Promise(resolve => requestAnimationFrame(() =>
          requestAnimationFrame(() => resolve(true))))
    )js";
    ASSERT_EQ(true,
              content::EvalJs(glic_host.webui_contents()->GetPrimaryMainFrame(),
                              kWaitForTwoFramesScript,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  }

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetAlignedGuestCenterInHost(glic_host, is_no_webview_).has_value();
  }));
  if (!is_no_webview_) {
    content::WaitForHitTestData(
        glic_host.webui_contents()->GetPrimaryMainFrame());
  }
  content::WaitForHitTestData(glic_host.GetGuestMainFrame());
}

gfx::Point GlicDragAndDropTestBase::GetGuestCenterInHost(Host& glic_host) {
  std::optional<gfx::Point> center;
  EXPECT_TRUE(base::test::RunUntil([&]() {
    center = GetAlignedGuestCenterInHost(glic_host, is_no_webview_);
    return center.has_value();
  }));
  return center.value_or(gfx::Point());
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

content::WebContents*
GlicDragAndDropTestBase::SetupSourceTabWithDraggableImage() {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  content::WebContents* source_wc = tab ? tab->GetContents() : nullptr;
  if (!source_wc) {
    ADD_FAILURE() << "No active web contents";
    return nullptr;
  }
  if (!content::NavigateToURL(
          source_wc, embedded_test_server()->GetURL(
                         "a.com", "/drag_and_drop/image_source.html"))) {
    ADD_FAILURE() << "NavigateToURL failed";
    return nullptr;
  }
  source_wc->Focus();

  // Fix the image src so it's draggable and wait for it to load, lay out, and
  // paint before querying its bounds or sending input events, so that
  // renderer-side and browser-side paint-holding do not drop the initial
  // mousedown event.
  GURL img_url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/cors-allowed.jpg");
  auto result = content::EvalJs(source_wc, content::JsReplace(
                                               R"js(
                       new Promise(resolve => {
                         const img = document.querySelector('img');
                         const waitForPaint = () => {
                           requestAnimationFrame(() =>
                               requestAnimationFrame(() => resolve(true)));
                         };
                         img.onload = waitForPaint;
                         img.onerror = () => resolve(false);
                         img.src = $1;
                         if (img.complete && img.naturalWidth > 0) {
                           waitForPaint();
                         }
                       })
                     )js",
                                               img_url.spec()));
  if (result != true) {
    ADD_FAILURE() << "Image did not load successfully: " << result;
    return nullptr;
  }
#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)
  content::WaitForResizeComplete(source_wc);
#endif
  content::WaitForCopyableViewInWebContents(source_wc);
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(source_wc);
  content::WaitForHitTestData(source_wc->GetPrimaryMainFrame());
  return source_wc;
}

bool GlicDragAndDropTestBase::SimulateMouseDownAndWait(
    content::WebContents* source_wc,
    const gfx::Point& point) {
  if (!content::ExecJs(source_wc, R"js(
        window.__mouseDownReceived = false;
        document.querySelector('img').addEventListener('mousedown', () => {
          window.__mouseDownReceived = true;
        }, {once: true});
      )js")) {
    return false;
  }

  constexpr int kMaxAttempts = 50;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    if (attempt > 0) {
      source_wc->Focus();
      content::SimulateMouseEvent(source_wc,
                                  blink::WebInputEvent::Type::kMouseUp,
                                  blink::WebMouseEvent::Button::kLeft, point);
    }
    content::SimulateMouseEvent(source_wc,
                                blink::WebInputEvent::Type::kMouseDown,
                                blink::WebMouseEvent::Button::kLeft, point);
    if (content::EvalJs(source_wc, R"js(
          new Promise(resolve => {
            if (window.__mouseDownReceived) {
              resolve(true);
              return;
            }
            const img = document.querySelector('img');
            const onDown = () => {
              clearTimeout(timer);
              resolve(true);
            };
            const timer = setTimeout(() => {
              img.removeEventListener('mousedown', onDown);
              resolve(Boolean(window.__mouseDownReceived));
            }, 50);
            img.addEventListener('mousedown', onDown, {once: true});
          })
        )js") == true) {
      return true;
    }
  }
  return false;
}

void GlicDragAndDropTestBase::SimulateMouseDragFromImage(
    content::WebContents* source_wc) {
  // Convert the image center from layout-viewport CSS pixels to visual-viewport
  // widget DIPs so `SimulateMouseEvent` hits `<img>` even when Mobile Android
  // scales down wide-viewport pages (`visualViewport.scale < 1`).
  content::EvalJsResult coords_result = content::EvalJs(source_wc, R"js(
    const img = document.querySelector('img');
    const rect = img.getBoundingClientRect();
    const vv = window.visualViewport;
    const scale = vv ? vv.scale : 1;
    [(rect.left + rect.width / 2 - (vv ? vv.offsetLeft : 0)) * scale,
     (rect.top + rect.height / 2 - (vv ? vv.offsetTop : 0)) * scale]
  )js");
  ASSERT_TRUE(coords_result.is_list()) << coords_result;
  const base::ListValue& coords = coords_result.ExtractList();
  ASSERT_EQ(coords.size(), 2u);

  gfx::Point drag_start_point = gfx::ToRoundedPoint(
      gfx::PointF(coords[0].GetDouble(), coords[1].GetDouble()));
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

}  // namespace glic
