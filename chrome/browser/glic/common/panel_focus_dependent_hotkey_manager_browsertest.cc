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
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/metrics/user_action_tester.h"
#include "printing/buildflags/buildflags.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/printing/test_print_preview_observer.h"
#include "chrome/browser/printing/test_print_view_manager_for_request_preview.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class PanelFocusDependentPrintHotkeyBrowserTestBase : public GlicBrowserTest {
 protected:
  printing::TestPrintViewManagerForRequestPreview* CreatePrintManager(
      content::WebContents* web_contents,
      base::test::TestFuture<void>& print_future) {
    printing::TestPrintViewManagerForRequestPreview::CreateForWebContents(
        web_contents);
    auto* manager =
        printing::TestPrintViewManagerForRequestPreview::FromWebContents(
            web_contents);
    if (manager) {
      manager->set_quit_closure(print_future.GetCallback());
    }
    return manager;
  }

  bool ProcessBrowserPrintAccelerator() {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        GetBrowser()->GetWindow()->GetNativeWindow());
    CHECK(widget);
    views::FocusManager* focus_manager = widget->GetFocusManager();
    CHECK(focus_manager);
    return focus_manager->ProcessAccelerator(
        GetAccelerator(LocalHotkeyManager::Command::kPrint));
  }
};

class PanelFocusDependentPrintHotkeyBrowserTest
    : public PanelFocusDependentPrintHotkeyBrowserTestBase {
 public:
  PanelFocusDependentPrintHotkeyBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicKeyboardPrint);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class PanelFocusDependentPrintHotkeyDisabledBrowserTest
    : public PanelFocusDependentPrintHotkeyBrowserTestBase {
 public:
  PanelFocusDependentPrintHotkeyDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicKeyboardPrint);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PanelFocusDependentPrintHotkeyBrowserTest,
                       PrintHotkeyFloaty) {
  base::UserActionTester user_action_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  base::test::TestFuture<void> tab_print_future;
  auto* tab_print_manager = CreatePrintManager(tab_contents, tab_print_future);
  ASSERT_TRUE(tab_print_manager);

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  base::test::TestFuture<void> guest_print_future;
  auto* guest_print_manager =
      CreatePrintManager(guest_contents, guest_print_future);
  ASSERT_TRUE(guest_print_manager);

  views::View* glic_view = instance->GetActiveEmbedderGlicViewForTesting();
  ASSERT_TRUE(glic_view);
  ASSERT_TRUE(glic_view->GetWidget());

  // Even when printing is disabled by policy so the accelerator returns false,
  // pressing the print shortcut on Floaty still records the user action.
  GetProfile()->GetPrefs()->SetBoolean(::prefs::kPrintingEnabled, false);
  EXPECT_FALSE(glic_view->GetWidget()->GetFocusManager()->ProcessAccelerator(
      GetAccelerator(LocalHotkeyManager::Command::kPrint)));
  EXPECT_FALSE(guest_print_future.IsReady());
  EXPECT_FALSE(tab_print_future.IsReady());
  EXPECT_EQ(1, user_action_tester.GetActionCount("Glic.PrintHotKey.Floaty"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.SidePanel"));

  GetProfile()->GetPrefs()->SetBoolean(::prefs::kPrintingEnabled, true);
  EXPECT_TRUE(glic_view->GetWidget()->GetFocusManager()->ProcessAccelerator(
      GetAccelerator(LocalHotkeyManager::Command::kPrint)));

  ASSERT_TRUE(guest_print_future.Wait());
  EXPECT_TRUE(guest_print_manager->print_preview_rfh());
  EXPECT_FALSE(tab_print_future.IsReady());
  EXPECT_FALSE(tab_print_manager->print_preview_rfh());
  EXPECT_EQ(2, user_action_tester.GetActionCount("Glic.PrintHotKey.Floaty"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.SidePanel"));
}

IN_PROC_BROWSER_TEST_F(PanelFocusDependentPrintHotkeyBrowserTest,
                       PrintHotkeySidePanel) {
  base::UserActionTester user_action_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  base::test::TestFuture<void> tab_print_future;
  auto* tab_print_manager = CreatePrintManager(tab_contents, tab_print_future);
  ASSERT_TRUE(tab_print_manager);

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  base::test::TestFuture<void> guest_print_future;
  auto* guest_print_manager =
      CreatePrintManager(guest_contents, guest_print_future);
  ASSERT_TRUE(guest_print_manager);

  printing::TestPrintPreviewObserver preview_observer(/*wait_for_loaded=*/true);
  EXPECT_TRUE(ProcessBrowserPrintAccelerator());

  content::WebContents* preview_dialog =
      preview_observer.WaitUntilPreviewIsReadyAndReturnPreviewDialog();
  ASSERT_TRUE(preview_dialog);
  ASSERT_TRUE(guest_print_future.Wait());
  EXPECT_TRUE(guest_print_manager->print_preview_rfh());
  EXPECT_FALSE(tab_print_future.IsReady());
  EXPECT_FALSE(tab_print_manager->print_preview_rfh());
  EXPECT_EQ(1, user_action_tester.GetActionCount("Glic.PrintHotKey.SidePanel"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.Floaty"));

  // Pressing the print shortcut again while Glic's Print Preview modal dialog
  // is focused should be consumed rather than falling through to BrowserView
  // and printing the active tab.
  base::test::TestFuture<void> guest_print_again_future;
  guest_print_manager->set_quit_closure(guest_print_again_future.GetCallback());
  preview_dialog->Focus();
  content::InputEventAckWaiter print_key_ack(
      preview_dialog->GetPrimaryMainFrame()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kRawKeyDown);
  content::SimulateKeyPress(preview_dialog, ui::DomKey::FromCharacter('p'),
                            ui::DomCode::US_P, ui::VKEY_P,
                            /*control=*/!BUILDFLAG(IS_MAC), /*shift=*/false,
                            /*alt=*/false, /*command=*/BUILDFLAG(IS_MAC));
  print_key_ack.Wait();
  EXPECT_FALSE(tab_print_future.IsReady());
  EXPECT_FALSE(tab_print_manager->print_preview_rfh());
  EXPECT_FALSE(guest_print_again_future.IsReady());
}

IN_PROC_BROWSER_TEST_F(PanelFocusDependentPrintHotkeyBrowserTest,
                       ThreeDotMenuPrintsActiveTabWhileSidePanelFocused) {
  base::UserActionTester user_action_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));
  ASSERT_TRUE(instance->GetActiveEmbedder());
  ASSERT_TRUE(instance->GetActiveEmbedder()->HasFocus());

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  base::test::TestFuture<void> tab_print_future;
  auto* tab_print_manager = CreatePrintManager(tab_contents, tab_print_future);
  ASSERT_TRUE(tab_print_manager);

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  base::test::TestFuture<void> guest_print_future;
  auto* guest_print_manager =
      CreatePrintManager(guest_contents, guest_print_future);
  ASSERT_TRUE(guest_print_manager);

  // Chrome's three-dot menu Print... (`chrome::Print`) should continue to print
  // the active tab even while the Glic side panel holds focus, without emitting
  // Glic print hotkey actions.
  chrome::Print(GetBrowser());
  ASSERT_TRUE(tab_print_future.Wait());
  EXPECT_TRUE(tab_print_manager->print_preview_rfh());
  EXPECT_FALSE(guest_print_future.IsReady());
  EXPECT_FALSE(guest_print_manager->print_preview_rfh());
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.SidePanel"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.Floaty"));
}

IN_PROC_BROWSER_TEST_F(PanelFocusDependentPrintHotkeyBrowserTest,
                       PrintHotkeySidePanelCrashedGuestConsumesShortcut) {
  base::UserActionTester user_action_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  base::test::TestFuture<void> tab_print_future;
  auto* tab_print_manager = CreatePrintManager(tab_contents, tab_print_future);
  ASSERT_TRUE(tab_print_manager);

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  content::CrashTab(guest_contents);
  ASSERT_TRUE(guest_contents->IsCrashed());

  views::View* glic_view = instance->GetActiveEmbedderGlicViewForTesting();
  ASSERT_TRUE(glic_view);
  glic_view->RequestFocus();
  ASSERT_TRUE(instance->GetActiveEmbedder());
  ASSERT_TRUE(instance->GetActiveEmbedder()->HasFocus());

  EXPECT_TRUE(ProcessBrowserPrintAccelerator());
  EXPECT_FALSE(tab_print_future.IsReady());
  EXPECT_FALSE(tab_print_manager->print_preview_rfh());
  EXPECT_EQ(1, user_action_tester.GetActionCount("Glic.PrintHotKey.SidePanel"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.Floaty"));
}

IN_PROC_BROWSER_TEST_F(PanelFocusDependentPrintHotkeyBrowserTest,
                       PrintHotkeySidePanelUnfocusedPrintsActiveTab) {
  base::UserActionTester user_action_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  tab_contents->Focus();
  ASSERT_OK(RunUntilEqual<bool>(
      [&]() {
        return instance->GetActiveEmbedder() &&
               instance->GetActiveEmbedder()->HasFocus();
      },
      false, "Timed out waiting for Glic to lose focus"));

  base::test::TestFuture<void> tab_print_future;
  auto* tab_print_manager = CreatePrintManager(tab_contents, tab_print_future);
  ASSERT_TRUE(tab_print_manager);

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  base::test::TestFuture<void> guest_print_future;
  auto* guest_print_manager =
      CreatePrintManager(guest_contents, guest_print_future);
  ASSERT_TRUE(guest_print_manager);

  const bool handled = ProcessBrowserPrintAccelerator();
#if BUILDFLAG(IS_MAC)
  // On macOS, Cmd+P is routed through the Cocoa main menu rather than
  // BrowserView's FocusManager accelerator table.
  EXPECT_FALSE(handled);
#else
  EXPECT_TRUE(handled);
  ASSERT_TRUE(tab_print_future.Wait());
  EXPECT_TRUE(tab_print_manager->print_preview_rfh());
#endif
  EXPECT_FALSE(guest_print_future.IsReady());
  EXPECT_FALSE(guest_print_manager->print_preview_rfh());
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.SidePanel"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.Floaty"));
}

IN_PROC_BROWSER_TEST_F(PanelFocusDependentPrintHotkeyDisabledBrowserTest,
                       PrintHotkeyFloatyDisabled) {
  base::UserActionTester user_action_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  base::test::TestFuture<void> tab_print_future;
  auto* tab_print_manager = CreatePrintManager(tab_contents, tab_print_future);
  ASSERT_TRUE(tab_print_manager);

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  base::test::TestFuture<void> guest_print_future;
  auto* guest_print_manager =
      CreatePrintManager(guest_contents, guest_print_future);
  ASSERT_TRUE(guest_print_manager);

  views::View* glic_view = instance->GetActiveEmbedderGlicViewForTesting();
  ASSERT_TRUE(glic_view);
  ASSERT_TRUE(glic_view->GetWidget());
  EXPECT_FALSE(glic_view->GetWidget()->GetFocusManager()->ProcessAccelerator(
      GetAccelerator(LocalHotkeyManager::Command::kPrint)));

  EXPECT_FALSE(guest_print_future.IsReady());
  EXPECT_FALSE(guest_print_manager->print_preview_rfh());
  EXPECT_FALSE(tab_print_future.IsReady());
  EXPECT_FALSE(tab_print_manager->print_preview_rfh());
  EXPECT_EQ(1, user_action_tester.GetActionCount("Glic.PrintHotKey.Floaty"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.SidePanel"));
}

IN_PROC_BROWSER_TEST_F(PanelFocusDependentPrintHotkeyDisabledBrowserTest,
                       PrintHotkeySidePanelDisabledPrintsActiveTab) {
  base::UserActionTester user_action_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  base::test::TestFuture<void> tab_print_future;
  auto* tab_print_manager = CreatePrintManager(tab_contents, tab_print_future);
  ASSERT_TRUE(tab_print_manager);

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  base::test::TestFuture<void> guest_print_future;
  auto* guest_print_manager =
      CreatePrintManager(guest_contents, guest_print_future);
  ASSERT_TRUE(guest_print_manager);

  const bool handled = ProcessBrowserPrintAccelerator();
#if BUILDFLAG(IS_MAC)
  // On macOS, Cmd+P is routed through the Cocoa main menu rather than
  // BrowserView's FocusManager accelerator table.
  EXPECT_FALSE(handled);
#else
  EXPECT_TRUE(handled);
  ASSERT_TRUE(tab_print_future.Wait());
  EXPECT_TRUE(tab_print_manager->print_preview_rfh());
#endif
  EXPECT_FALSE(guest_print_future.IsReady());
  EXPECT_FALSE(guest_print_manager->print_preview_rfh());
  EXPECT_EQ(1, user_action_tester.GetActionCount("Glic.PrintHotKey.SidePanel"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("Glic.PrintHotKey.Floaty"));
}

IN_PROC_BROWSER_TEST_F(PanelFocusDependentPrintHotkeyDisabledBrowserTest,
                       ModalDialogConsumesPrintShortcutWhenFeatureDisabled) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  ASSERT_OK(FocusGlic(instance));

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  base::test::TestFuture<void> tab_print_future;
  auto* tab_print_manager = CreatePrintManager(tab_contents, tab_print_future);
  ASSERT_TRUE(tab_print_manager);

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  base::test::TestFuture<void> guest_print_future;
  auto* guest_print_manager =
      CreatePrintManager(guest_contents, guest_print_future);
  ASSERT_TRUE(guest_print_manager);

  // Even when `kGlicKeyboardPrint` is disabled, Print Preview can still open on
  // Glic (e.g. via `window.print()` or the context menu). Pressing Ctrl+P /
  // Cmd+P while that modal dialog is focused should still be consumed rather
  // than leaking to `BrowserView` and printing the active tab.
  printing::TestPrintPreviewObserver preview_observer(/*wait_for_loaded=*/true);
  EXPECT_TRUE(instance->host().Print());
  content::WebContents* preview_dialog =
      preview_observer.WaitUntilPreviewIsReadyAndReturnPreviewDialog();
  ASSERT_TRUE(preview_dialog);
  ASSERT_TRUE(guest_print_future.Wait());

  preview_dialog->Focus();
  content::InputEventAckWaiter print_key_ack(
      preview_dialog->GetPrimaryMainFrame()->GetRenderWidgetHost(),
      blink::WebInputEvent::Type::kRawKeyDown);
  content::SimulateKeyPress(preview_dialog, ui::DomKey::FromCharacter('p'),
                            ui::DomCode::US_P, ui::VKEY_P,
                            /*control=*/!BUILDFLAG(IS_MAC), /*shift=*/false,
                            /*alt=*/false, /*command=*/BUILDFLAG(IS_MAC));
  print_key_ack.Wait();

  EXPECT_FALSE(tab_print_future.IsReady());
  EXPECT_FALSE(tab_print_manager->print_preview_rfh());
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace glic
