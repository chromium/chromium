// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "components/skills/public/skills_service.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}

class ConstrainedWebDialogDelegate;

namespace glic {
class GlicKeyedService;
}

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

  // Invokes the skill with skill_id in sidepanel.
  void InvokeSkill(std::string_view skill_id);

  ConstrainedWebDialogDelegate* GetDialogDelegateForTesting() {
    return dialog_delegate_.get();
  }

 protected:
  // Displays the glic panel.
  virtual void ShowGlicPanel();
  // Returns true if the glic client for the given tab is ready for context to
  // be sent.
  virtual bool IsClientReady();
  // Notify skill to invoke changed to the glic client
  virtual void NotifySkillToInvokeChanged();

  // Helper to retrieve the service on demand.
  glic::GlicKeyedService* GetGlicService();

 private:
  // Callback for when the dialog widget is closed by the user or system.
  void OnDialogClosed(const std::string& json_retval);

  // Starts a process that will notify skill to invoke changed once the glic
  // panel is ready.
  void NotifySkillToInvokeChangedWhenReady();
  // Called whenever notify skill to invoke is completed, successful or
  // otherwise. Stops the timer if it is running and clears state.
  void Reset();

  // Testing callback to be invoked when the dialog is closed.
  base::OnceClosure on_dialog_closed_callback_for_testing_;

  // The tab this controller belongs to.
  const raw_ref<tabs::TabInterface> tab_;

  raw_ptr<ConstrainedWebDialogDelegate> dialog_delegate_ = nullptr;

  ::ui::ScopedUnownedUserData<SkillsUiTabController> scoped_unowned_user_data_;

  base::RepeatingTimer glic_panel_ready_timer_;
  base::TimeTicks glic_panel_open_time_;
  std::string pending_skill_id_;
  base::WeakPtrFactory<SkillsUiTabController> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_
