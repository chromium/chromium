// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace glic {

using GlicZoomBrowserTest = GlicBrowserTest;

IN_PROC_BROWSER_TEST_F(GlicZoomBrowserTest, ZoomHotkeys) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  // Wait for WebUI to be ready.
  ASSERT_TRUE(WaitForWebUiState(mojom::WebUiState::kReady).has_value());

  // Get the focus manager for triggering accelerators.
  views::View* view = instance->GetActiveEmbedderGlicViewForTesting();
  ASSERT_TRUE(view);
  views::FocusManager* focus_manager = view->GetWidget()->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  // Initial zoom should be 1.0.
  EXPECT_DOUBLE_EQ(GetZoomLevel(instance), 1.0);

  // Trigger accelerator for zoom-in.
  base::span<const ui::Accelerator> zoom_in_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Command::kZoomIn);
  ASSERT_FALSE(zoom_in_accels.empty());
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in_accels[0]));

  // Verify zoom level increased.
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); }, 1.1,
                                  "Zoom level did not increase to 1.1"));

  // Trigger accelerator for zoom-out.
  base::span<const ui::Accelerator> zoom_out_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Command::kZoomOut);
  ASSERT_FALSE(zoom_out_accels.empty());
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out_accels[0]));
  // Verify zoom level decreased.
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); }, 1.0,
                                  "Zoom level did not decrease to 1.0"));

  // Trigger accelerator for zoom-reset.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in_accels[0]));
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); }, 1.1,
                                  "Zoom level did not increase to 1.1"));

  base::span<const ui::Accelerator> zoom_reset_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Command::kZoomReset);
  ASSERT_FALSE(zoom_reset_accels.empty());
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_reset_accels[0]));

  // Verify zoom level reset to 1.0.
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); }, 1.0,
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
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in_accels[1]));
  ASSERT_OK(RunUntilEqual<double>(
      [&]() { return GetZoomLevel(instance); }, 1.1,
      "Zoom level did not increase to 1.1 with Shift modifier"));

  // Trigger accelerator for zoom-out with Shift.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out_accels[1]));
  ASSERT_OK(RunUntilEqual<double>(
      [&]() { return GetZoomLevel(instance); }, 1.0,
      "Zoom level did not decrease to 1.0 with Shift modifier"));

  // Trigger accelerator for zoom-reset with Shift.
  // (First zoom in again so we can prove that reset scales it back.)
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in_accels[1]));
  ASSERT_OK(RunUntilEqual<double>(
      [&]() { return GetZoomLevel(instance); }, 1.1,
      "Zoom level did not increase to 1.1 with Shift modifier"));

  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_reset_accels[1]));
  ASSERT_OK(RunUntilEqual<double>(
      [&]() { return GetZoomLevel(instance); }, 1.0,
      "Zoom level did not reset to 1.0 with Shift modifier"));
}

IN_PROC_BROWSER_TEST_F(GlicZoomBrowserTest, ZoomScroll) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  // Wait for WebUI to be ready.
  ASSERT_TRUE(WaitForWebUiState(mojom::WebUiState::kReady).has_value());

  views::View* generic_view = instance->GetActiveEmbedderGlicViewForTesting();
  ASSERT_TRUE(generic_view);
  GlicView* glic_view = static_cast<GlicView*>(generic_view);

  // Initial zoom level should be 1.0.
  EXPECT_DOUBLE_EQ(GetZoomLevel(instance), 1.0);

  // Simulate Ctrl+Wheel up (zoom in) via ContentsZoomChange(true) and verify
  // zoom level increased.
  glic_view->ContentsZoomChange(true);
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); }, 1.1,
                                  "Zoom level did not increase to 1.1"));

  // Simulate another scroll and verify zoom level increased.
  glic_view->ContentsZoomChange(true);
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); },
                                  1.25, "Zoom level did not increase to 1.25"));

  // Simulate Ctrl+Wheel down (zoom out) via ContentsZoomChange(false) and
  // verify zoom level decreased.
  glic_view->ContentsZoomChange(false);
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); }, 1.1,
                                  "Zoom level did not decrease to 1.1"));
}

IN_PROC_BROWSER_TEST_F(GlicZoomBrowserTest, ZoomHotkeysPersisted) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  ASSERT_OK(FocusGlic(instance));

  // Initial zoom should be 1.0.
  EXPECT_DOUBLE_EQ(GetZoomLevel(instance), 1.0);

  // Trigger accelerator for zoom-in.
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);

  // Verify zoom level increased to 1.1.
  ASSERT_OK(RunUntilEqual<double>([&]() { return GetZoomLevel(instance); }, 1.1,
                                  "Zoom level did not increase to 1.1"));

  // Force a reload of Glic WebUI to trigger re-initialization from the
  // persisted zoom.
  content::WebContents* webui_contents = instance->host().webui_contents();
  webui_contents->GetController().Reload(content::ReloadType::NORMAL,
                                         /*check_for_repost=*/true);
  ASSERT_TRUE(content::WaitForLoadStop(webui_contents));
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  // Verify restored zoom is 1.1.
  EXPECT_DOUBLE_EQ(GetZoomLevel(instance), 1.1);

  // Trigger zoom-in again.
  TriggerHotkey(LocalHotkeyManager::Command::kZoomIn);

  // Verify zoom level increased to 1.25. If the bug exists, this will fail
  // because the zoom level will remain stuck at 1.1.
  ASSERT_OK(RunUntilEqual<double>(
      [&]() { return GetZoomLevel(instance); }, 1.25,
      "Zoom level did not increase to 1.25 after restore"));
}

}  // namespace glic
