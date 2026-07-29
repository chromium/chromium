// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace glic {

namespace {

class PanelFocusDependentHotkeyManagerBrowserTest : public GlicBrowserTest {
 public:
  PanelFocusDependentHotkeyManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicClientZoomControl);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PanelFocusDependentHotkeyManagerBrowserTest,
                       CloseHotkeyEscKey) {
  // TODO(b/538579840): Escape hotkey is intercepted by Android back navigation
  // logic.
  SKIP_NEEDS_ANDROID_IMPL(
      "b/538579840: Escape key intercepted by Android system back button");
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  // Wait for the webview client to load to verify that hotkeys work with the
  // webview.
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  TriggerHotkey(LocalHotkeyManager::Command::kClose);

  // Verify Glic is closed.
  ASSERT_TRUE(WaitForGlicClose(instance).has_value());
}

IN_PROC_BROWSER_TEST_F(PanelFocusDependentHotkeyManagerBrowserTest,
                       ZoomHotkeys) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  // Wait for the webview client to load to verify that hotkeys work with the
  // webview.
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  const double initial_zoom = GetZoomLevel(instance);

  // Trigger Zoom In.
  ASSERT_OK(FocusGlic(instance));
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);
  ASSERT_OK(
      RunUntilGreaterThan<double>([&]() { return GetZoomLevel(instance); },
                                  initial_zoom, "Zoom level did not increase"));
  // Wait for frame submission to ensure the renderer input router is rebound
  // before triggering the next hotkey.
  ASSERT_OK(WaitForGuestFrameSubmission(instance));

  const double zoomed_in = GetZoomLevel(instance);

  // Trigger Zoom Out.
  ASSERT_OK(FocusGlic(instance));
  TriggerHotkey(LocalHotkeyManager::Command::kZoomOut);
  ASSERT_OK(RunUntilLessThan<double>([&]() { return GetZoomLevel(instance); },
                                     zoomed_in, "Zoom level did not decrease"));
  // Wait for frame submission to ensure the renderer input router is rebound
  // before triggering the next hotkey.
  ASSERT_OK(WaitForGuestFrameSubmission(instance));

  // Trigger Zoom In again, then Zoom Reset.
  ASSERT_OK(FocusGlic(instance));
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);
  ASSERT_OK(RunUntilGreaterThan<double>(
      [&]() { return GetZoomLevel(instance); }, initial_zoom,
      "Zoom level did not increase again"));
  // Wait for frame submission to ensure the renderer input router is rebound
  // before triggering the next hotkey.
  ASSERT_OK(WaitForGuestFrameSubmission(instance));

  ASSERT_OK(FocusGlic(instance));
  TriggerHotkey(LocalHotkeyManager::Command::kZoomReset);
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); },
                                  initial_zoom,
                                  "Zoom level did not reset to initial"));
}

class PanelFocusDependentHotkeyManagerZoomDisabledBrowserTest
    : public GlicBrowserTest {
 public:
  PanelFocusDependentHotkeyManagerZoomDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kGlicClientZoomControl);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PanelFocusDependentHotkeyManagerZoomDisabledBrowserTest,
                       ZoomHotkeysDisabledByFlag) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  // Wait for the webview client to load to verify that hotkeys work with the
  // webview.
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  const double initial_zoom = GetZoomLevel(instance);

  // Triggering the shortcut should not zoom the Glic panel itself.
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);

  // Wait to verify that the zoom level did not change.
  WaitForDuration(base::Milliseconds(300));
  EXPECT_DOUBLE_EQ(GetZoomLevel(instance), initial_zoom);
}

}  // namespace
}  // namespace glic
