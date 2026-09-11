// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/geic/geic_enabling.h"
#include "chrome/browser/geic/geic_pwc_manager.h"
#include "chrome/browser/geic/geic_view.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_utils.h"

namespace geic {

class GeicZoomBrowserTest : public InProcessBrowserTest {
 public:
  GeicZoomBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {pwc::mojom::features::kPrivilegedWebContents}, {features::kGlic});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(geic::switches::kGeicEnabled);
    command_line->AppendSwitch(::switches::kDisableWebSecurity);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  GURL GetGuestURL() {
    return embedded_test_server()->GetURL("localhost", "/title1.html");
  }

  void OpenGeicSidePanel() {
    GeicPwcManager::GetOrCreateForProfile(browser()->GetProfile(),
                                          GetGuestURL());
    SidePanelUI* side_panel_ui = SidePanelUI::From(browser());
    ASSERT_TRUE(side_panel_ui);
    side_panel_ui->Show(SidePanelEntryId::kGeic);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return side_panel_ui->IsSidePanelShowing() &&
             side_panel_ui->IsSidePanelEntryShowing(
                 SidePanelEntryKey(SidePanelEntryId::kGeic));
    }));
  }

  content::WebContents* GetGeicWebContents() {
    auto* tab = browser()->tab_strip_model()->GetActiveTab();
    if (!tab) {
      return nullptr;
    }
    auto* geic_manager =
        GeicPwcManager::GetOrCreateForProfile(browser()->GetProfile());
    if (!geic_manager) {
      return nullptr;
    }
    return geic_manager->GetOrCreateWebContentsForTab(tab);
  }

  GeicView* GetGeicView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    if (!browser_view || !browser_view->side_panel()) {
      return nullptr;
    }
    return views::AsViewClass<GeicView>(
        browser_view->side_panel()->GetViewByID(GeicView::kGeicWebViewId));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that when GEiC side panel loads, ZoomController is attached,
// in ZOOM_MODE_ISOLATED, and at default zoom (1.0).
IN_PROC_BROWSER_TEST_F(GeicZoomBrowserTest,
                       ZoomControllerAttachedAndDefaultZoom) {
  OpenGeicSidePanel();
  content::WebContents* geic_contents = GetGeicWebContents();
  ASSERT_TRUE(geic_contents);
  ASSERT_TRUE(content::WaitForLoadStop(geic_contents));

  auto* zoom_controller = zoom::ZoomController::FromWebContents(geic_contents);
  ASSERT_TRUE(zoom_controller);
  EXPECT_EQ(zoom_controller->zoom_mode(),
            zoom::ZoomController::ZOOM_MODE_ISOLATED);
  EXPECT_TRUE(zoom_controller->IsAtDefaultZoom());
  EXPECT_DOUBLE_EQ(
      blink::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel()), 1.0);
}

// Verifies that keyboard shortcuts (Cmd/Ctrl +, Cmd/Ctrl -, Cmd/Ctrl 0)
// zoom in, zoom out, and reset zoom on the GEiC WebContents without affecting
// the main browser tab.
IN_PROC_BROWSER_TEST_F(GeicZoomBrowserTest, KeyboardZoomShortcutsAndReset) {
  OpenGeicSidePanel();
  content::WebContents* geic_contents = GetGeicWebContents();
  ASSERT_TRUE(geic_contents);
  ASSERT_TRUE(content::WaitForLoadStop(geic_contents));

  GeicView* geic_view = GetGeicView();
  ASSERT_TRUE(geic_view);
  geic_view->RequestFocus();

  auto* zoom_controller = zoom::ZoomController::FromWebContents(geic_contents);
  ASSERT_TRUE(zoom_controller);
  const double initial_factor =
      blink::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
  EXPECT_DOUBLE_EQ(initial_factor, 1.0);

  // Main tab zoom should start at 1.0.
  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* main_zoom = zoom::ZoomController::FromWebContents(main_contents);
  ASSERT_TRUE(main_zoom);
  EXPECT_DOUBLE_EQ(blink::ZoomLevelToZoomFactor(main_zoom->GetZoomLevel()),
                   1.0);

  views::FocusManager* focus_manager = geic_view->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  // 1. Attempting Zoom Out at zero state (1.0) must NOT decrease size.
  ui::Accelerator zoom_out(ui::VKEY_OEM_MINUS, ui::EF_PLATFORM_ACCELERATOR);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.0, 0.001);

  // Main tab zoom must NOT be affected.
  EXPECT_NEAR(blink::ZoomLevelToZoomFactor(main_zoom->GetZoomLevel()), 1.0,
              0.001);

  // 2. Zoom In: allowed exactly 5 times from zero state (1.0).
  // Step 1: 1.1
  ui::Accelerator zoom_in(ui::VKEY_OEM_PLUS, ui::EF_PLATFORM_ACCELERATOR);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.1, 0.001);

  // Step 2: 1.25
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.25, 0.001);

  // Step 3: 1.5
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.5, 0.001);

  // Step 4: 1.75
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.75, 0.001);

  // Step 5: 2.0 (maximum)
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 2.0, 0.001);

  // 3. Attempting a 6th Zoom In should be clamped at 2.0.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 2.0, 0.001);

  // Main tab zoom must still be 1.0.
  EXPECT_NEAR(blink::ZoomLevelToZoomFactor(main_zoom->GetZoomLevel()), 1.0,
              0.001);

  // 4. Zoom Out step-by-step down to 1.0.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.75, 0.001);

  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.5, 0.001);

  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.25, 0.001);

  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.1, 0.001);

  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.0, 0.001);

  // Zooming Out again at 1.0 does not decrease size.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.0, 0.001);

  // 5. Reset Zoom: Zoom in, then Cmd/Ctrl 0 to reset.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.1, 0.001);

  ui::Accelerator zoom_reset(ui::VKEY_0, ui::EF_PLATFORM_ACCELERATOR);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_reset));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.0, 0.001);
}

// Verifies that unhandled keyboard events from the GEiC WebContents route
// correctly to zoom actions.
IN_PROC_BROWSER_TEST_F(GeicZoomBrowserTest,
                       WebContentsUnhandledKeyboardEventRouting) {
  OpenGeicSidePanel();
  content::WebContents* geic_contents = GetGeicWebContents();
  ASSERT_TRUE(geic_contents);
  ASSERT_TRUE(content::WaitForLoadStop(geic_contents));

  GeicView* geic_view = GetGeicView();
  ASSERT_TRUE(geic_view);
  geic_view->RequestFocus();

  auto* zoom_controller = zoom::ZoomController::FromWebContents(geic_contents);
  ASSERT_TRUE(zoom_controller);
  EXPECT_DOUBLE_EQ(
      blink::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel()), 1.0);

  // Simulate unhandled keyboard event from WebContents for Zoom In.
  input::NativeWebKeyboardEvent event_in(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
#if BUILDFLAG(IS_MAC)
  event_in.SetModifiers(blink::WebInputEvent::kMetaKey);
#else
  event_in.SetModifiers(blink::WebInputEvent::kControlKey);
#endif
  event_in.windows_key_code = ui::VKEY_OEM_PLUS;

  EXPECT_TRUE(geic_view->HandleKeyboardEvent(geic_contents, event_in));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.1, 0.001);

  // Simulate unhandled keyboard event from WebContents for Zoom Reset.
  input::NativeWebKeyboardEvent event_reset(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
#if BUILDFLAG(IS_MAC)
  event_reset.SetModifiers(blink::WebInputEvent::kMetaKey);
#else
  event_reset.SetModifiers(blink::WebInputEvent::kControlKey);
#endif
  event_reset.windows_key_code = ui::VKEY_0;

  EXPECT_TRUE(geic_view->HandleKeyboardEvent(geic_contents, event_reset));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.0, 0.001);

  // Simulate unhandled keyboard event from WebContents for Zoom Out at 1.0.
  input::NativeWebKeyboardEvent event_out(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, base::TimeTicks::Now());
#if BUILDFLAG(IS_MAC)
  event_out.SetModifiers(blink::WebInputEvent::kMetaKey);
#else
  event_out.SetModifiers(blink::WebInputEvent::kControlKey);
#endif
  event_out.windows_key_code = ui::VKEY_OEM_MINUS;

  EXPECT_TRUE(geic_view->HandleKeyboardEvent(geic_contents, event_out));
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.0, 0.001);
}

// Verifies that zoom level is persisted across closing and reopening the
// GEiC side panel for the same tab.
IN_PROC_BROWSER_TEST_F(GeicZoomBrowserTest,
                       ZoomPersistenceAcrossSidePanelToggle) {
  OpenGeicSidePanel();
  content::WebContents* geic_contents = GetGeicWebContents();
  ASSERT_TRUE(geic_contents);
  ASSERT_TRUE(content::WaitForLoadStop(geic_contents));

  GeicView* geic_view = GetGeicView();
  ASSERT_TRUE(geic_view);
  geic_view->RequestFocus();

  auto* zoom_controller = zoom::ZoomController::FromWebContents(geic_contents);
  ASSERT_TRUE(zoom_controller);

  // Zoom In
  views::FocusManager* focus_manager = geic_view->GetFocusManager();
  ASSERT_TRUE(focus_manager);
  ui::Accelerator zoom_in(ui::VKEY_OEM_PLUS, ui::EF_PLATFORM_ACCELERATOR);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in));
  const double zoomed_factor = geic_view->GetZoomFactor();
  EXPECT_NEAR(zoomed_factor, 1.1, 0.001);

  // Close the side panel.
  SidePanelUI* side_panel_ui = SidePanelUI::From(browser());
  side_panel_ui->Close();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !side_panel_ui->IsSidePanelShowing(); }));

  // Re-open the side panel.
  side_panel_ui->Show(SidePanelEntryId::kGeic);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_ui->IsSidePanelShowing(); }));

  // The WebContents should have preserved its zoom level!
  GeicView* reopened_view = GetGeicView();
  ASSERT_TRUE(reopened_view);
  EXPECT_NEAR(reopened_view->GetZoomFactor(), zoomed_factor, 0.001);
}

// Verifies that when focus is on the main tab instead of GEiC, GEiC does not
// intercept zoom shortcuts, and the main tab zooms instead.
IN_PROC_BROWSER_TEST_F(GeicZoomBrowserTest, FocusDependentRouting) {
  OpenGeicSidePanel();
  content::WebContents* geic_contents = GetGeicWebContents();
  ASSERT_TRUE(geic_contents);
  ASSERT_TRUE(content::WaitForLoadStop(geic_contents));

  GeicView* geic_view = GetGeicView();
  ASSERT_TRUE(geic_view);

  content::WebContents* main_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(main_contents);
  auto* main_zoom = zoom::ZoomController::FromWebContents(main_contents);
  ASSERT_TRUE(main_zoom);
  auto* geic_zoom = zoom::ZoomController::FromWebContents(geic_contents);
  ASSERT_TRUE(geic_zoom);

  // Focus the main tab.
  main_contents->Focus();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->contents_web_view()->RequestFocus();

  // GeicView should not handle accelerators while unfocused.
  EXPECT_FALSE(geic_view->CanHandleAccelerators());

  // Zooming the main browser tab should not affect GEiC zoom.
  chrome::ExecuteCommand(browser(), IDC_ZOOM_PLUS);
  EXPECT_GT(blink::ZoomLevelToZoomFactor(main_zoom->GetZoomLevel()), 1.0);
  EXPECT_NEAR(geic_view->GetZoomFactor(), 1.0, 0.001);

  // When GeicView requests focus, it becomes eligible to handle accelerators.
  geic_view->RequestFocus();
  EXPECT_TRUE(geic_view->CanHandleAccelerators());
}

}  // namespace geic
