// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UI_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UI_WINDOW_CONTROLLER_H_

#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
struct ToastParams;

namespace skills {

// Manages Chrome UI for Skills.
class SkillsUiWindowController {
 public:
  DECLARE_USER_DATA(SkillsUiWindowController);
  explicit SkillsUiWindowController(
      BrowserWindowInterface* browser_window_interface);
  ~SkillsUiWindowController();

  static SkillsUiWindowController* From(
      BrowserWindowInterface* browser_window_interface);

  // Called when we want to update UI after a skill has been saved.
  void OnSkillSaved(std::string_view skill_id, bool hide_toast_button = false);
  // Called after a skill has been deleted from the UI.
  void OnSkillDeleted(std::string_view skill_id);

  // Called after a skill deletion has been undone from the UI.
  void UndoLastSkillRemoval();
  // Invokes last saved skill in sidepanel.
  void InvokeLastSavedSkill();
  // Invokes the skill with skill_id in sidepanel.
  void InvokeSkill(std::string_view skill_id);

 private:
  // Shows after a skill is saved or deleted. Takes in toast params to display.
  void ShowSkillToast(ToastParams params);
  void OnToastClosed(const std::string& skill_id);

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  ::ui::ScopedUnownedUserData<SkillsUiWindowController> scoped_data_holder_;
  std::string last_saved_skill_id_;
  std::string last_deleted_skill_id_;
  std::set<std::string> pending_deletions_;

  base::WeakPtrFactory<SkillsUiWindowController> weak_factory_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UI_WINDOW_CONTROLLER_H_
