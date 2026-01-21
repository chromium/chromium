// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"

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

void SkillsUiController::OnSkillSaved(std::string_view skill_id) {
  last_saved_skill_id_ = skill_id;
  ShowSkillToast(ToastId::kSkillSaved);
}

void SkillsUiController::OnSkillDeleted() {
  ShowSkillToast(ToastId::kSkillDeleted);
}

void SkillsUiController::ShowSkillToast(ToastId toast_id) {
  ToastController* const controller =
      browser_window_interface_->GetFeatures().toast_controller();
  controller->MaybeShowToast(ToastParams(toast_id));
}

void SkillsUiController::InvokeLastSavedSkill() {
  InvokeSkill(last_saved_skill_id_);
}

void SkillsUiController::InvokeSkill(std::string_view skill_id) {
  // TODO(crbug.com/475549806): Implement.
}

}  // namespace skills
