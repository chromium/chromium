// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/util.h"

#include <algorithm>

#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "base/notreached.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

int ModifierDomCodeToEventFlag(ui::DomCode code) {
  switch (code) {
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
      return ui::EF_ALT_DOWN;
    case ui::DomCode::CAPS_LOCK:
      return ui::EF_CAPS_LOCK_ON;
    case ui::DomCode::META_LEFT:
    case ui::DomCode::META_RIGHT:
      return ui::EF_COMMAND_DOWN;
    case ui::DomCode::SHIFT_LEFT:
    case ui::DomCode::SHIFT_RIGHT:
      return ui::EF_SHIFT_DOWN;
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
      return ui::EF_CONTROL_DOWN;
    default:
      return ui::EF_NONE;
  }
}

bool IsSameDomCode(ui::DomCode a, ui::DomCode b) {
  return a == b ||
         (ModifierDomCodeToEventFlag(a) != ui::EF_NONE &&
          ModifierDomCodeToEventFlag(a) == ModifierDomCodeToEventFlag(b));
}

MouseAction ConvertToMouseActionEnum(const std::string& mouse_action) {
  if (mouse_action == kPrimaryClick) {
    return MouseAction::PRIMARY_CLICK;
  } else if (mouse_action == kSecondaryClick) {
    return MouseAction::SECONDARY_CLICK;
  } else if (mouse_action == kHoverMove) {
    return MouseAction::HOVER_MOVE;
  } else if (mouse_action == kPrimaryDragMove) {
    return MouseAction::PRIMARY_DRAG_MOVE;
  } else if (mouse_action == kSecondaryDragMove) {
    return MouseAction::SECONDARY_DRAG_MOVE;
  }
  return MouseAction::NONE;
}

InputElement* GetInputBindingByBindingOption(Action* action,
                                             BindingOption binding_option) {
  InputElement* input_binding = nullptr;
  switch (binding_option) {
    case BindingOption::kCurrent:
      input_binding = action->current_input();
      break;
    case BindingOption::kOriginal:
      input_binding = action->original_input();
      break;
    case BindingOption::kPending:
      input_binding = action->pending_input();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return input_binding;
}

std::string GetCurrentSystemVersion() {
  return kSystemVersionAlphaV2;
}

void ResetFocusTo(views::View* view) {
  DCHECK(view);
  if (auto* focus_manager = view->GetFocusManager()) {
    focus_manager->SetFocusedView(view);
  }
}

// For the keys that are caught by display overlay, check if they are reserved
// for special use.
bool IsReservedDomCode(ui::DomCode code) {
  switch (code) {
    // Audio, brightness key events won't be caught by display overlay so no
    // need to add them.
    // Used for mouse lock.
    case ui::DomCode::ESCAPE:
    // Used for traversing the views, which is also required by Accessibility.
    case ui::DomCode::TAB:
    // Don't support according to UX requirement.
    case ui::DomCode::BROWSER_BACK:
    case ui::DomCode::BROWSER_FORWARD:
    case ui::DomCode::BROWSER_REFRESH:
      return true;
    default:
      return false;
  }
}

bool ContainShortcutEventFlags(const ui::KeyEvent* key_event) {
  return key_event &&
         (key_event->flags() & (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));
}

void UpdateFlagAndProperty(aura::Window* window,
                           ash::ArcGameControlsFlag flag,
                           bool turn_on) {
  const ash::ArcGameControlsFlag flags =
      window->GetProperty(ash::kArcGameControlsFlagsKey);

  if (IsFlagSet(flags, flag) != turn_on) {
    window->SetProperty(ash::kArcGameControlsFlagsKey,
                        UpdateFlag(flags, flag, turn_on));
  }
}

bool IsBeta() {
  return ash::features::IsGameDashboardEnabled() &&
         arc::GetArcAndroidSdkVersionAsInt() > arc::kArcVersionP;
}

}  // namespace arc::input_overlay
