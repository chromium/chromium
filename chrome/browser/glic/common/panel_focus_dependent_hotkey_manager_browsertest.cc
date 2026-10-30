// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/test/browser_test.h"
#include "printing/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !BUILDFLAG(IS_ANDROID)
#include "base/functional/bind.h"
#include "chrome/browser/printing/test_print_view_manager_for_request_preview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW) && !BUILDFLAG(IS_ANDROID)

namespace glic {

namespace {

using PanelFocusDependentHotkeyManagerBrowserTest = GlicBrowserTest;

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

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PanelFocusDependentHotkeyManagerBrowserTest,
                       PrintHotkeyFloaty) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  printing::TestPrintViewManagerForRequestPreview::CreateForWebContents(
      tab_contents);
  auto* tab_print_manager =
      printing::TestPrintViewManagerForRequestPreview::FromWebContents(
          tab_contents);
  ASSERT_TRUE(tab_print_manager);
  bool tab_printed = false;
  tab_print_manager->set_quit_closure(
      base::BindOnce([](bool* printed) { *printed = true; }, &tab_printed));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  printing::TestPrintViewManagerForRequestPreview::CreateForWebContents(
      guest_contents);
  auto* guest_print_manager =
      printing::TestPrintViewManagerForRequestPreview::FromWebContents(
          guest_contents);
  ASSERT_TRUE(guest_print_manager);

  base::RunLoop run_loop;
  guest_print_manager->set_quit_closure(run_loop.QuitClosure());

  views::View* glic_view = instance->GetActiveEmbedderGlicViewForTesting();
  ASSERT_TRUE(glic_view);
  ASSERT_TRUE(glic_view->GetWidget());
  EXPECT_TRUE(glic_view->GetWidget()->GetFocusManager()->ProcessAccelerator(
      GetAccelerator(LocalHotkeyManager::Command::kPrint)));

  run_loop.Run();
  EXPECT_FALSE(tab_printed);
  EXPECT_EQ(nullptr, tab_print_manager->print_preview_rfh());
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW) && !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace glic
