// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/test_result.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen_info.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace glic {

namespace {

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

  content::RenderWidgetHostView* rwhv =
      guest_contents->GetRenderWidgetHostView();
  if (!rwhv) {
    return base::unexpected("Warmed guest RenderWidgetHostView is null");
  }
  if (rwhv->GetScreenInfo().rect.height() <= 0) {
    return base::unexpected("Warmed guest ScreenInfo height is <= 0");
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
  instance->host().Reload();
  RETURN_IF_ERROR(RunUntilEqual(
      [&]() { return instance->host().client_load_state(); },
      ClientLoadState::kReady, "ReloadAndWait: client did not become ready"));
  return RunUntilNotEqual(
      [&]() { return instance->host().web_client_contents(); }, old_contents,
      "ReloadAndWait: web_client_contents did not change after Reload");
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
  RETURN_IF_ERROR(
      RunUntilEqual([&]() { return new_instance->host().client_load_state(); },
                    ClientLoadState::kReady,
                    "SwitchConversationAndWait: client did not become ready"));
  return new_instance;
}

[[nodiscard]] TestResult<> WaitForClientViewVisible(
    GlicInstanceImpl* instance) {
  if (!instance) {
    return base::unexpected("instance is null");
  }
  RETURN_IF_ERROR(
      RunUntilNotNull([&]() { return instance->host().web_client_contents(); },
                      "web_client_contents is null"));
  content::WebContents* client_contents =
      instance->host().web_client_contents();
  RETURN_IF_ERROR(RunUntilEqual(
      [&]() { return client_contents->GetVisibility(); },
      content::Visibility::VISIBLE, "client_contents is not visible"));
  RETURN_IF_ERROR(RunUntilNotNull(
      [&]() { return client_contents->GetRenderWidgetHostView(); },
      "RenderWidgetHostView is null"));
  content::RenderWidgetHostView* rwhv =
      client_contents->GetRenderWidgetHostView();
  RETURN_IF_ERROR(RunUntilEqual([&]() { return rwhv->IsShowing(); }, true,
                                "RenderWidgetHostView is not showing"));
#if defined(USE_AURA)
  RETURN_IF_ERROR(RunUntilNotNull([&]() { return rwhv->GetNativeView(); },
                                  "NativeView is null"));
  RETURN_IF_ERROR(
      RunUntilEqual([&]() { return rwhv->GetNativeView()->IsVisible(); }, true,
                    "NativeView is not visible"));
#endif
  RETURN_IF_ERROR(
      RunUntilEqual([&]() { return !rwhv->GetVisibleViewportSize().IsEmpty(); },
                    true, "VisibleViewportSize is empty"));
  RETURN_IF_ERROR(
      RunUntilEqual([&]() { return rwhv->IsSurfaceAvailableForCopy(); }, true,
                    "Surface is not available for copy"));
  content::WaitForCopyableViewInWebContents(client_contents);

  if (rwhv->GetScreenInfo().rect.height() <= 0) {
    return base::unexpected("client_contents ScreenInfo height is <= 0");
  }
  if (content::EvalJs(client_contents, "window.screen.height").ExtractInt() <=
      0) {
    return base::unexpected("client_contents window.screen.height is <= 0");
  }

  content::WebContents* webui_contents = instance->host().webui_contents();
  if (webui_contents && webui_contents != client_contents) {
    RETURN_IF_ERROR(RunUntilEqual(
        [&]() { return webui_contents->GetVisibility(); },
        content::Visibility::VISIBLE, "webui_contents is not visible"));
    RETURN_IF_ERROR(RunUntilNotNull(
        [&]() { return webui_contents->GetRenderWidgetHostView(); },
        "webui RenderWidgetHostView is null"));
    content::RenderWidgetHostView* webui_rwhv =
        webui_contents->GetRenderWidgetHostView();
    RETURN_IF_ERROR(RunUntilEqual([&]() { return webui_rwhv->IsShowing(); },
                                  true,
                                  "webui RenderWidgetHostView is not showing"));
#if defined(USE_AURA)
    RETURN_IF_ERROR(
        RunUntilNotNull([&]() { return webui_rwhv->GetNativeView(); },
                        "webui NativeView is null"));
    RETURN_IF_ERROR(RunUntilEqual(
        [&]() { return webui_rwhv->GetNativeView()->IsVisible(); }, true,
        "webui NativeView is not visible"));
#endif
    RETURN_IF_ERROR(RunUntilEqual(
        [&]() { return !webui_rwhv->GetVisibleViewportSize().IsEmpty(); }, true,
        "webui VisibleViewportSize is empty"));
    RETURN_IF_ERROR(
        RunUntilEqual([&]() { return webui_rwhv->IsSurfaceAvailableForCopy(); },
                      true, "webui Surface is not available for copy"));
    content::WaitForCopyableViewInWebContents(webui_contents);
  }

  return base::ok();
}

}  // namespace

class GlicVisibilityBrowserTest : public GlicBrowserTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  GlicVisibilityBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicContentsInitiallyHidden, {}},
        {features::kGlicWebContentsWarming,
         {{features::kGlicWebContentsWarmingDelay.name, "24h"}}},
    };
    std::vector<base::test::FeatureRef> disabled_features = {
        features::kGlicWarming};
    if (GetParam()) {
      enabled_features.push_back({features::kGlicNoWebview, {}});
    } else {
      disabled_features.push_back(features::kGlicNoWebview);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(
        switches::kDisableBackgroundingOccludedWindowsForTesting);
  }

  void SetUpOnMainThread() override {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDisableBackgroundingOccludedWindowsForTesting);
    GlicBrowserTest::SetUpOnMainThread();
  }

  GlicWebContentsWarmingPool& pool() {
    return coordinator().GetWebContentsWarmingPoolForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicVisibilityBrowserTest, ReloadInFloatyRendersFrame) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitForGlicClient(instance));
  PreventDeletionOnClose(instance, "conv1");

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

INSTANTIATE_TEST_SUITE_P(All,
                         GlicVisibilityBrowserTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace glic
