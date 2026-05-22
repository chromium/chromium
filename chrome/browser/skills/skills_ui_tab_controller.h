// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "chrome/common/buildflags.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace tabs {
class TabInterface;
}

namespace views {
class Widget;
}  // namespace views

namespace glic {
class GlicKeyedService;
struct Target;
}

namespace skills {

struct Skill;
class SkillsDialogDelegate;

// A controller responsible for managing the skills dialog for the tab.
class SkillsUiTabController : public SkillsUiTabControllerInterface,
                              public SkillsDialogDelegate,
                              public views::WidgetObserver {
 public:
  explicit SkillsUiTabController(tabs::TabInterface& tab);
  ~SkillsUiTabController() override;
  DECLARE_USER_DATA(SkillsUiTabController);

  // Opens the skills dialog.
  void ShowDialog(Skill skill,
                  SkillsDialogEntryPoint entrypoint,
                  mojom::SkillsDialogType dialog_type,
                  std::unique_ptr<glic::Target> target) override;

  // Invokes the skill with skill_id in sidepanel.
  void InvokeSkill(std::string_view skill_id) override;

  // SkillsDialogDelegate override:
  void CloseDialog() override;
  void OnSkillSaved(const std::string& skill_id) override;
  void OnSkillDeleted(const std::string& skill_id) override;

  // views::WidgetObserver override:
  void OnWidgetDestroyed(views::Widget* widget) override;

  // Synchronously resets pointers to ensure immediate memory cleanup.
  void OnDialogClosing(views::Widget::ClosedReason reason);

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

  // Returns true if the skills dialog is currently being shown.
  bool IsShowing() const;

 protected:
  // Helper to retrieve a skill by ID.
  virtual const skills::Skill* GetSkill(std::string_view skill_id);
  // Helper to retrieve the service on demand.
  virtual glic::GlicKeyedService* GetGlicService();

 private:
  // Testing callback to be invoked when the dialog is closed.
  base::OnceClosure on_dialog_closed_callback_for_testing_;

  // The tab this controller belongs to.
  const raw_ref<tabs::TabInterface> tab_;

  // Subscription for tab detach events.
  base::CallbackListSubscription will_detach_subscription_;

  // The Delegate handles the dialog logic and owns the internal View.
  std::unique_ptr<views::DialogDelegate> dialog_delegate_;

  // The Widget represents the window.
  // CLIENT_OWNS_WIDGET ensures stability during tab reparenting/dragging.
  std::unique_ptr<views::Widget> dialog_widget_;

  ::ui::ScopedUnownedUserData<SkillsUiTabController> scoped_unowned_user_data_;

  std::unique_ptr<glic::Target> target_;
  std::string last_invoked_skill_id_for_testing_;

  // Caches the skill for which the dialog is currently shown.
  std::optional<skills::Skill> current_skill_;
  base::WeakPtrFactory<SkillsUiTabController> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_UI_TAB_CONTROLLER_H_
