// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {

class GlicZoomBrowserTest : public GlicBrowserTest,
                            public testing::WithParamInterface<bool> {
 public:
  GlicZoomBrowserTest() {
    if (IsNoWebview()) {
      SetUseHttpsForGlicUrl(true);
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlicNoWebview,
                                pwc::mojom::features::kPrivilegedWebContents},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kGlicNoWebview});
    }
  }

  bool IsNoWebview() const { return GetParam(); }
  bool IsWebview() const { return !IsNoWebview(); }

  [[nodiscard]] TestResult<> WaitForZoomFactor(
      GlicInstanceImpl* instance,
      double base_zoom,
      double relative_factor,
      std::string_view message = std::string_view()) {
    return RunUntilNear<double>([&]() { return GetZoomLevel(instance); },
                                base_zoom * relative_factor, 0.001, message);
  }

  [[nodiscard]] TestResult<> WaitForPrefZoomPercent(int expected_percent) {
    if (IsNoWebview()) {
      // In NoWebview mode, zoom is handled synchronously on the UI thread in
      // C++, so the pref is updated immediately without spinning the run loop.
      int actual = GetProfile()->GetPrefs()->GetInteger(prefs::kGlicZoomLevel);
      if (actual != expected_percent) {
        return base::unexpected(base::StringPrintf(
            "Expected pref %d but got %d", expected_percent, actual));
      }
      return base::ok();
    }
    // In Webview mode, zoom is applied by <webview> in JS while pref updates
    // are dispatched asynchronously via Mojo. Wait for the Mojo IPC.
    // TODO(b/534807813): Delete this helper function and switch callers to
    // direct synchronous EXPECT_EQ pref checks once Webview mode is eliminated.
    return RunUntilEqual<int>(
        [&]() {
          return GetProfile()->GetPrefs()->GetInteger(prefs::kGlicZoomLevel);
        },
        expected_percent);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicZoomBrowserTest, ZoomHotkeys) {
  if (IsWebview()) {
    // In Webview mode, zoom actions query an asynchronous getZoom() API on
    // <webview> to determine the next discrete zoom factor. When rapid hotkey
    // sequences are executed, asynchronous zoom queries race with in-flight
    // zoom changes and display scale factor calibrations, leading to stale zoom
    // reads (e.g. skipping a zoom factor or treating zoom-out as a no-op).
    // Webview mode is deprecated and will be removed once NoWebview is default
    // (b/534807813).
    GTEST_SKIP() << "Skipping on Webview due to async getZoom race condition";
  }

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  const double base_zoom = GetZoomLevel(instance);
  if (IsNoWebview()) {
    EXPECT_NEAR(base_zoom, 1.0, 0.001);
  }

  // Trigger accelerator for zoom-in.
  base::span<const ui::Accelerator> zoom_in_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Command::kZoomIn);
  ASSERT_FALSE(zoom_in_accels.empty());
  TriggerHotkey(zoom_in_accels[0]);

  // Verify zoom level increased.
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.1,
                              "Zoom level did not increase to 1.1"));

  // Trigger accelerator for zoom-out.
  base::span<const ui::Accelerator> zoom_out_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Command::kZoomOut);
  ASSERT_FALSE(zoom_out_accels.empty());
  TriggerHotkey(zoom_out_accels[0]);
  // Verify zoom level decreased.
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.0,
                              "Zoom level did not decrease to 1.0"));

  // Trigger accelerator for zoom-reset.
  TriggerHotkey(zoom_in_accels[0]);
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.1,
                              "Zoom level did not increase to 1.1"));

  base::span<const ui::Accelerator> zoom_reset_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Command::kZoomReset);
  ASSERT_FALSE(zoom_reset_accels.empty());
  TriggerHotkey(zoom_reset_accels[0]);

  // Verify zoom level reset to 1.0.
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.0,
                              "Zoom level did not reset to 1.0"));

  // Repeat with Shift variations of hotkeys.
  // Ensure that Shift variations exist in the accelerator arrays.
  ASSERT_GT(zoom_in_accels.size(), 1u);
  ASSERT_TRUE(zoom_in_accels[1].modifiers() & ui::EF_SHIFT_DOWN);
  ASSERT_GT(zoom_out_accels.size(), 1u);
  ASSERT_TRUE(zoom_out_accels[1].modifiers() & ui::EF_SHIFT_DOWN);
  ASSERT_GT(zoom_reset_accels.size(), 1u);
  ASSERT_TRUE(zoom_reset_accels[1].modifiers() & ui::EF_SHIFT_DOWN);

  // Trigger accelerator for zoom-in with Shift.
  TriggerHotkey(zoom_in_accels[1]);
  ASSERT_OK(WaitForZoomFactor(
      instance, base_zoom, 1.1,
      "Zoom level did not increase to 1.1 with Shift modifier"));

  // Trigger accelerator for zoom-out with Shift.
  TriggerHotkey(zoom_out_accels[1]);
  ASSERT_OK(WaitForZoomFactor(
      instance, base_zoom, 1.0,
      "Zoom level did not decrease to 1.0 with Shift modifier"));

  // Trigger accelerator for zoom-reset with Shift.
  // (First zoom in again so we can prove that reset scales it back.)
  TriggerHotkey(zoom_in_accels[1]);
  ASSERT_OK(WaitForZoomFactor(
      instance, base_zoom, 1.1,
      "Zoom level did not increase to 1.1 with Shift modifier"));

  TriggerHotkey(zoom_reset_accels[1]);
  ASSERT_OK(
      WaitForZoomFactor(instance, base_zoom, 1.0,
                        "Zoom level did not reset to 1.0 with Shift modifier"));
}

// Wheel-based page zoom is not supported on macOS (see
// WebContentsImpl::HandleWheelEvent).
#if BUILDFLAG(IS_MAC)
#define MAYBE_ZoomScroll DISABLED_ZoomScroll
#else
#define MAYBE_ZoomScroll ZoomScroll
#endif
IN_PROC_BROWSER_TEST_P(GlicZoomBrowserTest, MAYBE_ZoomScroll) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  const double base_zoom = GetZoomLevel(instance);
  if (IsNoWebview()) {
    EXPECT_NEAR(base_zoom, 1.0, 0.001);
  }

  // Simulate Ctrl+Wheel up (zoom in) and verify zoom level increased.
  TriggerCtrlWheel(instance, /*zoom_in=*/true);
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.1,
                              "Zoom level did not increase to 1.1"));

  // Simulate another scroll and verify zoom level increased.
  TriggerCtrlWheel(instance, /*zoom_in=*/true);
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.25,
                              "Zoom level did not increase to 1.25"));

  // Simulate Ctrl+Wheel down (zoom out) and verify zoom level decreased.
  TriggerCtrlWheel(instance, /*zoom_in=*/false);
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.1,
                              "Zoom level did not decrease to 1.1"));
}

IN_PROC_BROWSER_TEST_P(GlicZoomBrowserTest, ZoomHotkeysPersisted) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  ASSERT_OK(FocusGlic(instance));

  const double base_zoom = GetZoomLevel(instance);
  if (IsNoWebview()) {
    EXPECT_NEAR(base_zoom, 1.0, 0.001);
  }

  // Trigger accelerator for zoom-in.
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);

  // Verify zoom level increased to 1.1.
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.1,
                              "Zoom level did not increase to 1.1"));

  // Force a reload of Glic to trigger re-initialization from the
  // persisted zoom.
  content::WebContents* webui_contents = instance->host().webui_contents();
  webui_contents->GetController().Reload(content::ReloadType::NORMAL,
                                         /*check_for_repost=*/true);
  ASSERT_TRUE(content::WaitForLoadStop(webui_contents));
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  // Verify restored zoom is 1.1.
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.1,
                              "Zoom level did not restore to 1.1"));

  // Trigger zoom-in again.
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);

  // Verify zoom level increased to 1.25. If the bug exists, this will fail
  // because the zoom level will remain stuck at 1.1.
  ASSERT_OK(
      WaitForZoomFactor(instance, base_zoom, 1.25,
                        "Zoom level did not increase to 1.25 after restore"));
}

IN_PROC_BROWSER_TEST_P(GlicZoomBrowserTest, ZoomChangeCountMetric) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  const double base_zoom = GetZoomLevel(instance);
  if (IsNoWebview()) {
    EXPECT_NEAR(base_zoom, 1.0, 0.001);
  }

  // 1. Zoom in: 1.0 -> 1.1 (count = 1).
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.1));
  ASSERT_OK(WaitForPrefZoomPercent(110));

  // 2. Zoom in: 1.1 -> 1.25 (count = 2).
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.25));
  ASSERT_OK(WaitForPrefZoomPercent(125));

  // 3. Zoom reset: 1.25 -> 1.0 (count = 3).
  TriggerHotkey(LocalHotkeyManager::Command::kZoomReset);
  ASSERT_OK(WaitForZoomFactor(instance, base_zoom, 1.0));
  ASSERT_OK(WaitForPrefZoomPercent(100));

  // Redundant reset when already at 1.0 should NOT increment count.
  TriggerHotkey(LocalHotkeyManager::Command::kZoomReset);

  // Destroy the instance to trigger metric emission on close/destruction.
  coordinator().RemoveInstance(instance->id());

  histogram_tester.ExpectUniqueSample("Glic.Instance.ZoomChangeCount", 3, 1);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicZoomBrowserTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "NoWebview" : "Webview";
    });

}  // namespace glic
