// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

class ConstrainedWebDialogDelegate;

namespace skills {

struct Skill;

// A controller responsible for managing the skills dialog for the tab.
class SkillsUiTabController : public SkillsUiTabControllerInterface {
 public:
  explicit SkillsUiTabController(tabs::TabInterface& tab);
  ~SkillsUiTabController() override;
  DECLARE_USER_DATA(SkillsUiTabController);

  // Opens the skills dialog.
  void ShowDialog(const skills::Skill& skill) override;

  // Closes the dialog if it is currently open.
  void CloseDialog() override;

  // Called by the WebUI when a skill is successfully saved.
  // Delegates visual feedback to the Window Controller.
  void OnSkillSaved(const std::string& skill_id) override;

  void SetOnDialogClosedCallbackForTesting(base::OnceClosure callback) {
    on_dialog_closed_callback_for_testing_ = std::move(callback);
  }

  ConstrainedWebDialogDelegate* GetDialogDelegateForTesting() {
    return dialog_delegate_.get();
  }

 private:
  // Callback for when the dialog widget is closed by the user or system.
  void OnDialogClosed(const std::string& json_retval);

  // Testing callback to be invoked when the dialog is closed.
  base::OnceClosure on_dialog_closed_callback_for_testing_;

  // The tab this controller belongs to.
  const raw_ref<tabs::TabInterface> tab_;

  raw_ptr<ConstrainedWebDialogDelegate> dialog_delegate_ = nullptr;
  ::ui::ScopedUnownedUserData<SkillsUiTabController> scoped_unowned_user_data_;
  base::WeakPtrFactory<SkillsUiTabController> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_
