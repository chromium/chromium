// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_UI_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_UI_UTILS_H_

#include <memory>
#include <string>

#include "ui/events/keycodes/dom/dom_code.h"

namespace views {
class View;
}

namespace arc::input_overlay {

class Action;

// Create name tag with title and sub-title as:
// -----------
// |Title    |
// |Sub-title|
// -----------
std::unique_ptr<views::View> CreateNameTag(const std::u16string& title,
                                           const std::u16string& sub_title);

// Create key layout view ActionTap.
// -----
// | a |
// -----
std::unique_ptr<views::View> CreateActionTapEditForKeyboard(Action* action);

// Create key layout view for ActionMove.
// -------------
// |   | w |   |
// |-----------|
// | a | s | d |
// -------------
std::unique_ptr<views::View> CreateActionMoveEditForKeyboard(Action* action);

// Get text of |code| displayed on input mappings.
std::u16string GetDisplayText(const ui::DomCode code);

// Get the accessible name for displayed |text| showing on input mappings.
// Sometimes, |text| is a symbol.
std::u16string GetDisplayTextAccessibleName(const std::u16string& text);

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_UI_UTILS_H_
