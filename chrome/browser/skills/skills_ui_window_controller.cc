// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_window_controller.h"

#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/skills/public/skills_metrics.h"
#include "components/tabs/public/tab_interface.h"

namespace skills {

DEFINE_USER_DATA(SkillsUiWindowController);

SkillsUiWindowController::SkillsUiWindowController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_data_holder_(browser_window_interface->GetUnownedUserDataHost(),
                          *this) {}

SkillsUiWindowController::~SkillsUiWindowController() = default;

SkillsUiWindowController* SkillsUiWindowController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

void SkillsUiWindowController::OnSkillSaved(std::string_view skill_id) {
  last_saved_skill_id_ = skill_id;
  ShowSkillToast(ToastId::kSkillSaved);
}

void SkillsUiWindowController::OnSkillDeleted() {
  ShowSkillToast(ToastId::kSkillDeleted);
}

void SkillsUiWindowController::ShowSkillToast(ToastId toast_id) {
  ToastController* const controller =
      browser_window_interface_->GetFeatures().toast_controller();
  controller->MaybeShowToast(ToastParams(toast_id));
}

void SkillsUiWindowController::InvokeLastSavedSkill() {
  InvokeSkill(last_saved_skill_id_);
}

void SkillsUiWindowController::InvokeSkill(std::string_view skill_id) {
  if (skill_id.empty()) {
    return;
  }

  if (auto* active_tab = browser_window_interface_->GetActiveTabInterface()) {
    if (auto* tab_controller =
            skills::SkillsUiTabControllerInterface::From(active_tab)) {
      tab_controller->InvokeSkill(skill_id);
    }
  }
}

}  // namespace skills
