// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/toplevel_window.h"

#include "ash/shell.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace shell {
namespace {

struct SavedState {
  gfx::Rect bounds;
  ui::WindowShowState show_state;
};

// The last window state in ash_shell. We don't bother deleting
// this on shutdown.
SavedState* saved_state = NULL;

}  // namespace

ToplevelWindow::CreateParams::CreateParams()
    : can_resize(false), can_maximize(false), use_saved_placement(true) {}

// static
views::Widget* ToplevelWindow::CreateToplevelWindow(
    const CreateParams& params) {
  views::Widget* widget = views::Widget::CreateWindowWithContext(
      new ToplevelWindow(params), Shell::GetPrimaryRootWindow());
  widget->GetNativeView()->SetName("Examples:ToplevelWindow");
  WindowState* window_state = WindowState::Get(widget->GetNativeView());
  window_state->SetWindowPositionManaged(true);
  widget->Show();
  return widget;
}

// static
void ToplevelWindow::ClearSavedStateForTest() {
  delete saved_state;
  saved_state = NULL;
}

ToplevelWindow::ToplevelWindow(const CreateParams& params) : params_(params) {}

ToplevelWindow::~ToplevelWindow() = default;

void ToplevelWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetLocalBounds(), SK_ColorDKGRAY);
}

base::string16 ToplevelWindow::GetWindowTitle() const {
  return base::ASCIIToUTF16("Examples: Toplevel Window");
}

void ToplevelWindow::SaveWindowPlacement(const gfx::Rect& bounds,
                                         ui::WindowShowState show_state) {
  if (!saved_state)
    saved_state = new SavedState;
  saved_state->bounds = bounds;
  saved_state->show_state = show_state;
}

bool ToplevelWindow::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  bool is_saved_bounds = !!saved_state;
  if (saved_state && params_.use_saved_placement) {
    *bounds = saved_state->bounds;
    *show_state = saved_state->show_state;
  } else {
    // Initial default bounds.
    bounds->SetRect(10, 150, 300, 300);
  }
  WindowPositioner::GetBoundsAndShowStateForNewWindow(
      is_saved_bounds, *show_state, bounds, show_state);
  return true;
}

bool ToplevelWindow::CanResize() const {
  return params_.can_resize;
}

bool ToplevelWindow::CanMaximize() const {
  return params_.can_maximize;
}

bool ToplevelWindow::CanMinimize() const {
  return params_.can_maximize;
}

}  // namespace shell
}  // namespace ash
