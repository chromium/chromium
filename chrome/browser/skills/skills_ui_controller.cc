// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace skills {

DEFINE_USER_DATA(SkillsUiController);

SkillsUiController::SkillsUiController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_data_holder_(browser_window_interface->GetUnownedUserDataHost(),
                          *this) {}

SkillsUiController::~SkillsUiController() = default;

SkillsUiController* SkillsUiController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

void SkillsUiController::ShowDialog(std::string_view prompt) {
  // TODO(crbug.com/475589469): Implement this.
}

}  // namespace skills
