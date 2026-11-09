// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/test_result.h"
#include "chrome/browser/glic/widget/glic_floating_ui.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/screen_info.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#endif

namespace glic {

namespace {

constexpr char kCoopGuestPath[] = "/glic/browser_tests/coop_client.html";

std::unique_ptr<net::test_server::HttpResponse> HandleCoopGuestRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().GetPath() != kCoopGuestPath) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  // Serve with `Cross-Origin-Opener-Policy: same-origin-allow-popups` (matching
  // production gemini.google.com/glic) so navigating from initial `about:blank`
  // forces a cross-BrowsingInstance speculative RenderFrameHost and
  // RenderWidgetHostViewAura swap.
  response->AddCustomHeader("Cross-Origin-Opener-Policy",
                            "same-origin-allow-popups");
  response->set_content(R"html(<!DOCTYPE html>
<html>
<head>
  <link rel="modulepreload" href="./minimal_client.js">
  <link rel="modulepreload" href="../api_boot.js">
  <script>
    window.addEventListener('message', function onBootstrap(event) {
      if (event.origin === 'chrome://glic' &&
          event.data?.type === 'glic-bootstrap') {
        window.removeEventListener('message', onBootstrap);
        window.screenHeightAtBootstrap = window.screen.height;
      }
    });
  </script>
</head>
<body>
  <script type="module" src="./minimal_client.js"></script>
</body>
</html>)html");
  return response;
}

// Describes the visibility-relevant state of `instance` for inclusion in a
// test failure message. The WebContents visibility (VISIBLE vs OCCLUDED vs
// HIDDEN), the widget state, and the native occlusion state are the fields
// that distinguish "glic never showed the window" from "the OS reports the
// window occluded" (e.g. locked session on Windows bots).
std::string DescribeVisibilityState(GlicInstanceImpl* instance) {
  if (!instance) {
    return "instance=null";
  }
  std::string out;
  if (content::WebContents* contents = instance->host().web_client_contents()) {
    base::StrAppend(
        &out,
        {"client.visibility=",
         base::NumberToString(static_cast<int>(contents->GetVisibility())),
         "(0=HIDDEN,1=OCCLUDED,2=VISIBLE)"});
    if (content::RenderWidgetHostView* rwhv =
            contents->GetRenderWidgetHostView()) {
      base::StrAppend(&out,
                      {" rwhv.showing=", base::ToString(rwhv->IsShowing()),
                       " screen=", rwhv->GetScreenInfo().rect.ToString()});
    }
  } else {
    base::StrAppend(&out, {"client=null"});
  }
  GlicFloatingUi* floating_ui = instance->GetFloatingUiForTesting();
  base::WeakPtr<views::View> view =
      floating_ui ? floating_ui->GetView() : nullptr;
  views::Widget* widget = view ? view->GetWidget() : nullptr;
  if (widget) {
    base::StrAppend(&out,
                    {" widget.visible=", base::ToString(widget->IsVisible()),
                     " active=", base::ToString(widget->IsActive()),
                     " bounds=", widget->GetWindowBoundsInScreen().ToString()});
#if defined(USE_AURA)
    if (aura::Window* native = widget->GetNativeWindow()) {
      base::StrAppend(&out,
                      {" native.occlusion=",
                       base::UTF16ToUTF8(aura::Window::OcclusionStateToString(
                           native->GetOcclusionState()))});
    }
#endif
  } else {
    base::StrAppend(&out, {" widget=null"});
  }
  if (display::Screen* screen = display::Screen::Get()) {
    base::StrAppend(
        &out, {" display=", screen->GetPrimaryDisplay().bounds().ToString()});
  }
  return out;
}

// Like RETURN_IF_ERROR, but appends the visibility state to the error so it
// shows up in the gtest failure message.
#define RETURN_IF_ERROR_WITH_STATE(instance, expr)                 \
  do {                                                             \
    auto _result = (expr);                                         \
    if (!_result.has_value()) {                                    \
      return base::unexpected(                                     \
          base::StrCat({std::move(_result).error(), " [",          \
                        DescribeVisibilityState(instance), "]"})); \
    }                                                              \
  } while (0)

[[nodiscard]] TestResult<> WarmAndWaitForLoad(
    GlicWebContentsWarmingPool& pool) {
  if (!pool.MaybeStartWarming(GlicWarmingTrigger::kNudge)) {
    return base::unexpected("MaybeStartWarming returned false");
  }
  RETURN_IF_ERROR(
      RunUntilEqual([&]() { return pool.HasWarmedContainerForTesting(); }, true,
                    "Timed out waiting for warmed container"));
  RETURN_IF_ERROR(RunUntilNotNull(
      [&]() -> content::WebContents* {
        auto warmed_contents = pool.GetWarmedWebContents();
        return warmed_contents ? warmed_contents->guest_contents.get()
                               : nullptr;
      },
      "Timed out waiting for warmed guest_contents"));

  auto warmed_contents = pool.GetWarmedWebContents();
  if (!warmed_contents || !warmed_contents->guest_contents) {
    return base::unexpected("guest_contents is null after waiting");
  }
  content::WebContents* guest_contents = warmed_contents->guest_contents.get();
  if (!content::WaitForLoadStop(guest_contents)) {
    return base::unexpected("WaitForLoadStop failed for warmed guest contents");
  }
  RETURN_IF_ERROR(RunUntilEqual(
      [&]() {
        return content::EvalJs(guest_contents,
                               "typeof window.screenHeightAtBootstrap === "
                               "'number'")
            .ExtractBool();
      },
      true, "Timed out waiting for guest bootstrap message"));

  content::RenderWidgetHostView* rwhv =
      guest_contents->GetRenderWidgetHostView();
  if (!rwhv) {
    return base::unexpected("Warmed guest RenderWidgetHostView is null");
  }
  if (guest_contents->GetVisibility() != content::Visibility::HIDDEN) {
    return base::unexpected("Warmed guest visibility is not HIDDEN");
  }
  if (guest_contents->IsBeingCaptured()) {
    return base::unexpected("Warmed guest is unexpectedly being captured");
  }
  if (rwhv->IsShowing()) {
    return base::unexpected(
        "Warmed guest RenderWidgetHostView is unexpectedly showing");
  }
  if (rwhv->GetScreenInfo().rect.height() <= 0) {
    return base::unexpected("Warmed guest ScreenInfo height is <= 0");
  }
  if (content::EvalJs(guest_contents, "window.screenHeightAtBootstrap")
          .ExtractInt() <= 0) {
    return base::unexpected(
        "Warmed guest window.screenHeightAtBootstrap is <= 0");
  }
  if (content::EvalJs(guest_contents, "window.screen.height").ExtractInt() <=
      0) {
    return base::unexpected("Warmed guest window.screen.height is <= 0");
  }

  return base::ok();
}

[[nodiscard]] TestResult<> ReloadAndWait(GlicInstanceImpl* instance) {
  if (!instance) {
    return base::unexpected("instance is null");
  }
  content::WebContents* old_contents = instance->host().web_client_contents();
  if (!old_contents) {
    return base::unexpected("web_client_contents is null before Reload");
  }
  // Reload() synchronously destroys the old WebContents and creates a new one.
  // Observe destruction of the old contents rather than comparing pointers:
  // the allocator may reuse the freed slot, so the new WebContents can end up
  // at the same address as the old one (observed on MSan).
  content::WebContentsDestroyedWatcher destroyed_watcher(old_contents);
  instance->host().Reload();
  destroyed_watcher.Wait();
  if (!destroyed_watcher.IsDestroyed()) {
    return base::unexpected(
        "ReloadAndWait: old web_client_contents was not destroyed");
  }
  RETURN_IF_ERROR_WITH_STATE(
      instance, RunUntilNotNull(
                    [&]() { return instance->host().web_client_contents(); },
                    "ReloadAndWait: web_client_contents is null after Reload"));
  RETURN_IF_ERROR_WITH_STATE(
      instance,
      RunUntilEqual([&]() { return instance->host().client_load_state(); },
                    ClientLoadState::kReady,
                    "ReloadAndWait: client did not become ready"));
  return base::ok();
}

[[nodiscard]] TestResult<GlicInstanceImpl*> SwitchConversationAndWait(
    GlicInstanceCoordinatorImpl& coordinator,
    GlicInstanceImpl* from_instance,
    const std::string& conversation_id) {
  if (!from_instance) {
    return base::unexpected("from_instance is null");
  }
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = conversation_id;
  base::test::TestFuture<std::optional<mojom::SwitchConversationErrorReason>>
      switch_future;
  from_instance->host().SwitchConversation(std::move(info),
                                           switch_future.GetCallback());
  if (switch_future.Get().has_value()) {
    return base::unexpected("SwitchConversation returned an error");
  }

  auto* new_instance =
      static_cast<GlicInstanceImpl*>(coordinator.GetActiveInstance());
  if (!new_instance) {
    return base::unexpected("Active instance is null after switch");
  }
  if (new_instance == from_instance) {
    return base::unexpected("Active instance did not change after switch");
  }
  if (coordinator.GetInstanceWithFloaty() != new_instance ||
      !new_instance->IsDetached()) {
    return base::unexpected("New instance is not detached in floaty");
  }
  RETURN_IF_ERROR_WITH_STATE(
      new_instance,
      RunUntilEqual([&]() { return new_instance->host().client_load_state(); },
                    ClientLoadState::kReady,
                    "SwitchConversationAndWait: client did not become ready"));
  return new_instance;
}

[[nodiscard]] TestResult<> WaitForWebContentsRendered(
    GlicInstanceImpl* instance,
    content::WebContents* contents,
    std::string_view label) {
  RETURN_IF_ERROR_WITH_STATE(
      instance, RunUntilEqual([&]() { return contents->GetVisibility(); },
                              content::Visibility::VISIBLE,
                              base::StrCat({label, " is not visible"})));
  RETURN_IF_ERROR_WITH_STATE(
      instance,
      RunUntilNotNull([&]() { return contents->GetRenderWidgetHostView(); },
                      base::StrCat({label, " RenderWidgetHostView is null"})));
  content::RenderWidgetHostView* rwhv = contents->GetRenderWidgetHostView();
  RETURN_IF_ERROR_WITH_STATE(
      instance,
      RunUntilEqual(
          [&]() { return rwhv->IsShowing(); }, true,
          base::StrCat({label, " RenderWidgetHostView is not showing"})));
#if defined(USE_AURA)
  RETURN_IF_ERROR_WITH_STATE(
      instance, RunUntilNotNull([&]() { return rwhv->GetNativeView(); },
                                base::StrCat({label, " NativeView is null"})));
  RETURN_IF_ERROR_WITH_STATE(
      instance,
      RunUntilEqual([&]() { return rwhv->GetNativeView()->IsVisible(); }, true,
                    base::StrCat({label, " NativeView is not visible"})));
#endif
  RETURN_IF_ERROR_WITH_STATE(
      instance,
      RunUntilEqual([&]() { return !rwhv->GetVisibleViewportSize().IsEmpty(); },
                    true,
                    base::StrCat({label, " VisibleViewportSize is empty"})));
  RETURN_IF_ERROR_WITH_STATE(
      instance,
      RunUntilEqual(
          [&]() { return rwhv->IsSurfaceAvailableForCopy(); }, true,
          base::StrCat({label, " Surface is not available for copy"})));
  content::WaitForCopyableViewInWebContents(contents);
  return base::ok();
}

[[nodiscard]] TestResult<> WaitForClientViewVisible(
    GlicInstanceImpl* instance) {
  if (!instance) {
    return base::unexpected("instance is null");
  }
  RETURN_IF_ERROR_WITH_STATE(
      instance,
      RunUntilNotNull([&]() { return instance->host().web_client_contents(); },
                      "web_client_contents is null"));
  content::WebContents* client_contents =
      instance->host().web_client_contents();
  RETURN_IF_ERROR(
      WaitForWebContentsRendered(instance, client_contents, "client_contents"));

  content::RenderWidgetHostView* rwhv =
      client_contents->GetRenderWidgetHostView();
  if (!rwhv || rwhv->GetScreenInfo().rect.height() <= 0) {
    return base::unexpected(
        base::StrCat({"client_contents ScreenInfo height is <= 0 [",
                      DescribeVisibilityState(instance), "]"}));
  }
  if (content::EvalJs(client_contents, "window.screen.height").ExtractInt() <=
      0) {
    return base::unexpected(
        base::StrCat({"client_contents window.screen.height is <= 0 [",
                      DescribeVisibilityState(instance), "]"}));
  }

  content::WebContents* webui_contents = instance->host().webui_contents();
  if (webui_contents && webui_contents != client_contents) {
    RETURN_IF_ERROR(
        WaitForWebContentsRendered(instance, webui_contents, "webui_contents"));
  }

  return base::ok();
}

}  // namespace

// NOTE: Keep the default --disable-backgrounding-occluded-windows-for-testing
// switch set by BrowserTestBase. On the Windows 11 browser_tests fleet the
// session is locked / display off, and NativeWindowOcclusionTrackerWin then
// reports every window as OCCLUDED regardless of geometry (see
// native_window_occlusion_tracker_win.cc, "If the screen is locked or off").
// With native occlusion active, WebContents in the floating window never
// become VISIBLE there and the test times out deterministically. The
// regression this test guards (ScreenInfo / window.screen.height being 0
// during warmup, reload, conversation switch, or floaty pop-out with
// GlicContentsInitiallyHidden) does not depend on OS occlusion tracking.
class GlicVisibilityBrowserTest : public GlicBrowserTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  GlicVisibilityBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicContentsInitiallyHidden, {}},
        {features::kGlicWebContentsWarming,
         {{features::kGlicWebContentsWarmingDelay.name, "168h"}}},
    };
    std::vector<base::test::FeatureRef> disabled_features = {
        features::kGlicWarming, ::features::kPopulateUnattachedAuraScreenInfo};
    if (GetParam()) {
      enabled_features.push_back({features::kGlicNoWebview, {}});
    } else {
      disabled_features.push_back(features::kGlicNoWebview);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);

    embedded_https_test_server().RegisterRequestHandler(
        base::BindRepeating(&HandleCoopGuestRequest));
    SetGlicPagePath(kCoopGuestPath);
  }

  GlicWebContentsWarmingPool& pool() {
    return coordinator().GetWebContentsWarmingPoolForTesting();
  }

  void VerifySidePanelAndFloatyTransitions() {
    ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
    ASSERT_OK(WaitForGlicClient(instance));
    PreventDeletionOnClose(instance, "conv1");
    EXPECT_FALSE(instance->IsDetached());
    ASSERT_OK(WaitForClientViewVisible(instance));
    EXPECT_GT(content::EvalJs(instance->host().web_client_contents(),
                              "window.screenHeightAtBootstrap")
                  .ExtractInt(),
              0);

    tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
    ASSERT_TRUE(tab);
    instance->Detach(*tab);
    ASSERT_TRUE(RunUntil([&]() { return instance->IsDetached(); },
                         "Timed out waiting for instance to detach"));
    ASSERT_OK(WaitForClientViewVisible(instance));

    // Re-attach back to Side Panel.
    instance->Attach(tab->GetHandle());
    ASSERT_TRUE(RunUntil([&]() { return !instance->IsDetached(); },
                         "Timed out waiting for instance to re-attach"));
    ASSERT_OK(WaitForClientViewVisible(instance));

    // Pop out to Floaty a second time.
    instance->Detach(*tab);
    ASSERT_TRUE(RunUntil([&]() { return instance->IsDetached(); },
                         "Timed out waiting for instance to detach again"));
    ASSERT_OK(WaitForClientViewVisible(instance));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicVisibilityBrowserTest, ReloadInFloatyRendersFrame) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitForGlicClient(instance));
  PreventDeletionOnClose(instance, "conv1");
  // Baseline before any transition: if this fails, the floaty itself is not
  // visible on this bot and reload/switch are not the culprit.
  EXPECT_OK(WaitForClientViewVisible(instance));

  // 1. Cold reload (empty warming pool).
  ASSERT_FALSE(pool().HasWarmedContainerForTesting());
  ASSERT_OK(ReloadAndWait(instance));
  EXPECT_OK(WaitForClientViewVisible(instance));

  // 2. Warm reload (pre-warmed container ready in pool).
  ASSERT_OK(WarmAndWaitForLoad(pool()));
  ASSERT_OK(ReloadAndWait(instance));
  EXPECT_OK(WaitForClientViewVisible(instance));
}

IN_PROC_BROWSER_TEST_P(GlicVisibilityBrowserTest,
                       SwitchConversationInFloatyRendersFrame) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance_a,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitForGlicClient(instance_a));
  PreventDeletionOnClose(instance_a, "conv_a");
  // Baseline: see comment in ReloadInFloatyRendersFrame.
  EXPECT_OK(WaitForClientViewVisible(instance_a));

  // 1. Cold switch (empty warming pool).
  ASSERT_FALSE(pool().HasWarmedContainerForTesting());
  ASSERT_OK_AND_ASSIGN(
      GlicInstanceImpl * instance_b,
      SwitchConversationAndWait(coordinator(), instance_a, "conv_b"));
  PreventDeletionOnClose(instance_b, "conv_b");
  EXPECT_OK(WaitForClientViewVisible(instance_b));

  // 2. Warm switch (pre-warmed container ready in pool).
  ASSERT_OK(WarmAndWaitForLoad(pool()));
  ASSERT_OK_AND_ASSIGN(
      GlicInstanceImpl * instance_c,
      SwitchConversationAndWait(coordinator(), instance_b, "conv_c"));
  EXPECT_OK(WaitForClientViewVisible(instance_c));
}

IN_PROC_BROWSER_TEST_P(GlicVisibilityBrowserTest, PopOutToFloatyRendersFrame) {
  ASSERT_OK(WarmAndWaitForLoad(pool()));
  VerifySidePanelAndFloatyTransitions();
}

IN_PROC_BROWSER_TEST_P(GlicVisibilityBrowserTest,
                       ColdPopOutToFloatyRendersFrame) {
  ASSERT_FALSE(pool().HasWarmedContainerForTesting());
  VerifySidePanelAndFloatyTransitions();
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicVisibilityBrowserTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace glic
