// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_event_targeter.h"

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/check.h"
#include "ui/aura/window.h"

namespace ash {

AppListEventTargeter::AppListEventTargeter(AppListViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

AppListEventTargeter::~AppListEventTargeter() = default;

bool AppListEventTargeter::SubtreeShouldBeExploredForEvent(
    aura::Window* window,
    const ui::LocatedEvent& event) {
  if (delegate_ && !delegate_->CanProcessEventsOnApplistViews())
    return false;

  if (window->GetProperty(assistant::ui::kOnlyAllowMouseClickEvents)) {
    if (event.type() != ui::EventType::kMousePressed &&
        event.type() != ui::EventType::kMouseReleased) {
      return false;
    }
  }

  return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
}

}  // namespace ash
