// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UTIL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/arc_game_controls_flag.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace arc::input_overlay {

class Action;
class InputElement;

// Arrow key move distance per key press event.
constexpr int kArrowKeyMoveDistance = 2;

// Gets the event flags for the modifier domcode. Return ui::DomCode::NONE if
// `code` is not modifier DomCode.
int ModifierDomCodeToEventFlag(ui::DomCode code);
bool IsSameDomCode(ui::DomCode a, ui::DomCode b);
// Convert mouse action strings to enum values.
MouseAction ConvertToMouseActionEnum(const std::string& mouse_action);

// Return the input binding filtered by `binding_option` in `action`.
InputElement* GetInputBindingByBindingOption(Action* action,
                                             BindingOption binding_option);

// Return the current running version of Game controls. If it is not set, it's
// Alpha version. Otherwise, it is AlphaV2+ version.
std::string GetCurrentSystemVersion();

// Reset the focus to `view`.
void ResetFocusTo(views::View* view);

// Return true if `code` is not allowed to bind.
bool IsReservedDomCode(ui::DomCode code);

// Turn `flag` on or off for `window` property `ash::kArcGameControlsFlagsKey`.
void UpdateFlagAndProperty(aura::Window* window,
                           ash::ArcGameControlsFlag flag,
                           bool turn_on);

// TODO(b/253646354): This will be removed when removing the flag.
bool IsBeta();
// TODO(b/288155422): This will be removed when removing the flag.
bool IsGameDashboardFlagOn();

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UTIL_H_
