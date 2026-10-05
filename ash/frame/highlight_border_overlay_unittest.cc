// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/highlight_border_overlay.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "chromeos/ui/base/app_types.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"

namespace ash {

using HighlightBorderOverlayTest = AshTestBase;

// Tests that the highlight border is shown in clamshell mode unless the window
// is fullscreen, and in tablet mode only if the window is floated or PiP.
TEST_F(HighlightBorderOverlayTest, Visibility) {
  std::unique_ptr<aura::Window> window =
      CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  HighlightBorderOverlay overlay(widget, /*delegate=*/nullptr);
  const ui::Layer* layer =
      FindLayerWithName(widget->GetLayer(), "HighlightBorderOverlay");
  ASSERT_TRUE(layer);
  EXPECT_TRUE(layer->visible());

  widget->SetFullscreen(true);
  EXPECT_FALSE(layer->visible());
  widget->SetFullscreen(false);
  EXPECT_TRUE(layer->visible());

  // Entering tablet mode maximizes the window.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(layer->visible());

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent float_event(WM_EVENT_FLOAT);
  window_state->OnWMEvent(&float_event);
  ASSERT_TRUE(window_state->IsFloated());
  EXPECT_TRUE(layer->visible());

  // `TabletModeWindowState` doesn't handle PiP events, so enter PiP in
  // clamshell mode. PiP windows stay in PiP in tablet mode.
  TabletModeControllerTestApi().LeaveTabletMode();
  const WMEvent pip_event(WM_EVENT_PIP);
  window_state->OnWMEvent(&pip_event);
  TabletModeControllerTestApi().EnterTabletMode();
  ASSERT_TRUE(window_state->IsPip());
  EXPECT_TRUE(layer->visible());
}

}  // namespace ash
