// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/first_run_helper_impl.h"

#include <memory>
#include <utility>

#include "ash/app_list/views/app_list_view.h"
#include "ash/first_run/desktop_cleaner.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace ash {

// static
std::unique_ptr<FirstRunHelper> FirstRunHelper::Start(
    base::OnceClosure on_cancelled) {
  return std::make_unique<FirstRunHelperImpl>(std::move(on_cancelled));
}

FirstRunHelperImpl::FirstRunHelperImpl(base::OnceClosure on_cancelled)
    : on_cancelled_(std::move(on_cancelled)),
      cleaner_(std::make_unique<DesktopCleaner>()) {
  Shell::Get()->session_controller()->AddObserver(this);
}

FirstRunHelperImpl::~FirstRunHelperImpl() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  // Ensure the tray is closed.
  CloseTrayBubble();
}

gfx::Rect FirstRunHelperImpl::GetAppListButtonBounds() {
  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  HomeButton* home_button = shelf->shelf_widget()->GetHomeButton();
  return home_button->GetBoundsInScreen();
}

gfx::Rect FirstRunHelperImpl::OpenTrayBubble() {
  UnifiedSystemTray* tray = Shell::Get()
                                ->GetPrimaryRootWindowController()
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  tray->ShowBubble(false /* show_by_click */);
  return tray->GetBubbleBoundsInScreen();
}

void FirstRunHelperImpl::CloseTrayBubble() {
  Shell::Get()
      ->GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->unified_system_tray()
      ->CloseBubble();
}

void FirstRunHelperImpl::OnLockStateChanged(bool locked) {
  Cancel();
}

void FirstRunHelperImpl::OnChromeTerminating() {
  Cancel();
}

void FirstRunHelperImpl::Cancel() {
  if (!on_cancelled_.is_null())
    std::move(on_cancelled_).Run();
}

}  // namespace ash
