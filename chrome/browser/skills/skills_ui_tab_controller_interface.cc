// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"

#include "base/logging.h"
#include "components/tabs/public/tab_interface.h"

namespace skills {

DEFINE_USER_DATA(SkillsUiTabControllerInterface);

SkillsUiTabControllerInterface::SkillsUiTabControllerInterface(
    tabs::TabInterface& tab)
    : scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}
SkillsUiTabControllerInterface::~SkillsUiTabControllerInterface() = default;

// static
SkillsUiTabControllerInterface* SkillsUiTabControllerInterface::From(
    tabs::TabInterface* tab) {
  if (!tab) {
    LOG(ERROR) << "Tab does not exist.";
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

}  // namespace skills
