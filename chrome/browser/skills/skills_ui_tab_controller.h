// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "components/skills/public/skill.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/interaction/element_identifier.h"

class BrowserWindowInterface;
class Profile;

namespace glic {
class GlicKeyedService;
class GlicModal;
struct Target;
}  // namespace glic

namespace skills {

// A controller responsible for managing the skills dialog for the tab.
class SkillsUiTabController : public SkillsUiTabControllerInterface,
                              public SkillsDialogDelegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kSkillsDialogElementId);

  explicit SkillsUiTabController(tabs::TabInterface& tab);
  ~SkillsUiTabController() override;

  // Opens the skills dialog.
  void ShowDialog(Skill skill,
                  SkillsDialogEntryPoint entrypoint,
                  mojom::SkillsDialogType dialog_type,
                  std::unique_ptr<glic::Target> target) override;

  // Invokes the skill with skill_id, skill_name, skill_icon in sidepanel.
  // skill_name and skill_icon are unused for skills v1.
  void InvokeSkill(std::string_view skill_id,
                   std::string_view skill_name,
                   std::string_view skill_icon,
                   bool auto_submit) override;
  void SendPrompt(std::string_view prompt, bool new_conversation) override;

  // SkillsDialogDelegate & SkillsUiTabControllerInterface override:
  void CloseDialog() override;
  void OnSkillSaved(const std::string& skill_id) override;
  void OnSkillSaved(const std::string& skill_id,
                    const std::string& skill_name,
                    const std::string& skill_icon) override;
  void OnSkillDeleted(const std::string& skill_id) override;
  void OnSkillDeleted(const std::string& skill_id,
                      base::OnceCallback<void(bool)> callback) override;
  void InvokeSkill(std::string_view skill_id) override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;

  // Synchronously resets pointers to ensure immediate memory cleanup.
  void OnDialogClosing();

  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  void SetOnDialogClosedCallbackForTesting(base::OnceClosure callback) {
    on_dialog_closed_callback_for_testing_ = std::move(callback);
  }

  const std::optional<skills::Skill>& GetCurrentSkillForTesting() const {
    return current_skill_;
  }

  const std::string& GetLastInvokedSkillIdForTesting() const {
    return last_invoked_skill_id_for_testing_;
  }

  void SetTargetForTesting(std::unique_ptr<glic::Target> target);

  // Returns true if the skills dialog is currently being shown.
  bool IsShowing() const override;

 protected:
  // Helper to retrieve a skill by ID.
  virtual const skills::Skill* GetSkill(std::string_view skill_id);
  // Helper to retrieve the service on demand.
  virtual glic::GlicKeyedService* GetGlicService();
  // Helper to retrieve the profile on demand.
  virtual Profile* GetProfile();

 private:
  // Testing callback to be invoked when the dialog is closed.
  base::OnceClosure on_dialog_closed_callback_for_testing_;

  // The tab this controller belongs to.
  const raw_ref<tabs::TabInterface> tab_;

  // Subscription for tab detach events.
  base::CallbackListSubscription will_detach_subscription_;

  // Active modal dialog handle when open.
  std::unique_ptr<glic::GlicModal> dialog_modal_;

  std::unique_ptr<glic::Target> target_;
  std::string last_invoked_skill_id_for_testing_;

  // Caches the skill for which the dialog is currently shown.
  std::optional<skills::Skill> current_skill_;
  base::WeakPtrFactory<SkillsUiTabController> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_
