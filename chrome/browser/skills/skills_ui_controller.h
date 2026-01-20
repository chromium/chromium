// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UI_CONTROLLER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UI_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace skills {

// Manages Chrome UI for Skills.
class SkillsUiController {
 public:
  DECLARE_USER_DATA(SkillsUiController);
  explicit SkillsUiController(BrowserWindowInterface* browser_window_interface);
  ~SkillsUiController();

  static SkillsUiController* From(
      BrowserWindowInterface* browser_window_interface);

  void ShowDialog(std::string_view prompt);
  // Called when we want to update UI after a skill has been saved.
  void OnSkillSaved(std::string_view skill_id);

  // Shows after a skill is saved.
  void ShowSkillSavedToast();
  // Invokes last saved skill in sidepanel.
  void InvokeLastSavedSkill();
  // Invokes the skill with skill_id in sidepanel.
  void InvokeSkill(std::string_view skill_id);

 private:
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  ::ui::ScopedUnownedUserData<SkillsUiController> scoped_data_holder_;
  std::string last_saved_skill_id_;
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UI_CONTROLLER_H_
