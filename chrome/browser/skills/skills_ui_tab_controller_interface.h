// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_INTERFACE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

namespace skills {

struct Skill;

class SkillsUiTabControllerInterface {
 public:
  DECLARE_USER_DATA(SkillsUiTabControllerInterface);
  explicit SkillsUiTabControllerInterface(tabs::TabInterface& tab);
  virtual ~SkillsUiTabControllerInterface();

  // Retrieves an SkillsUiTabController from the provided tab, or
  // nullptr if it does not exist.
  static SkillsUiTabControllerInterface* From(tabs::TabInterface* tab);

  // Opens the skills dialog.
  virtual void ShowDialog(const skills::Skill& skill) = 0;

  // Closes the dialog if it is currently open.
  virtual void CloseDialog() = 0;

  // Called by the WebUI when a skill is successfully saved.
  // Delegates visual feedback to the Window Controller.
  virtual void OnSkillSaved(const std::string& skill_id) = 0;

 private:
  ::ui::ScopedUnownedUserData<SkillsUiTabControllerInterface>
      scoped_unowned_user_data_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_INTERFACE_H_
