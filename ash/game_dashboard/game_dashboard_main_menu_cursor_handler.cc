// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_main_menu_cursor_handler.h"

#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/shell.h"
#include "chromeos/ui/frame/frame_header.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

GameDashboardMainMenuCursorHandler::GameDashboardMainMenuCursorHandler(
    GameDashboardContext* context)
    : context_(context), cursor_manager_(Shell::Get()->cursor_manager()) {
  CHECK(context_);
  CHECK(cursor_manager_);
  // Save current state.
  cursor_visibility_to_restore_ = cursor_manager_->IsCursorVisible();
  cursor_to_restore_ = cursor_manager_->GetCursor();
  // Set desired state.
  cursor_manager_->ShowCursor();
  cursor_manager_->SetCursor(ui::mojom::CursorType::kPointer);
  cursor_manager_->LockCursor();
}

GameDashboardMainMenuCursorHandler::~GameDashboardMainMenuCursorHandler() {
  cursor_manager_->UnlockCursor();
  // Restore state.
  cursor_visibility_to_restore_ ? cursor_manager_->ShowCursor()
                                : cursor_manager_->HideCursor();
  cursor_manager_->SetCursor(cursor_to_restore_);
}

void GameDashboardMainMenuCursorHandler::OnMouseEvent(ui::MouseEvent* event) {
  // Propagate the `event` if it's inside the window's
  // `FrameCaptionButtonContainerView`.
  if (!IsEventInWindowFrameHeader(*event)) {
    event->StopPropagation();
    event->SetHandled();
  }
}

bool GameDashboardMainMenuCursorHandler::IsEventInWindowFrameHeader(
    const ui::LocatedEvent& event) {
  // TODO(b/324268128): Update the logic to handle a LaCrOS window's
  // FrameHeader.
  if (auto* frame_header = chromeos::FrameHeader::Get(
          views::Widget::GetWidgetForNativeWindow(context_->game_window()))) {
    const auto* frame_header_view = frame_header->view();
    // For apps that use pointer lock, `event.target()` always returns the
    // window that has acquired the pointer lock. This pretarget handler cannot
    // reliably figure out whether the mouse cursor is located over the game
    // window's frame header or not. As a workaround, the logic is using the
    // event's screen location to determine whether the mouse event is within
    // the painted region of the frame header.
    const auto frame_header_painted_region =
        gfx::Rect(frame_header_view->GetBoundsInScreen().origin(),
                  gfx::Size(frame_header_view->width(),
                            frame_header->GetHeaderHeightForPainting()));
    return frame_header_painted_region.Contains(
        event.target()->GetScreenLocation(event));
  }
  return false;
}

}  // namespace ash
