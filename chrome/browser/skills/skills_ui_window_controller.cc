// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_window_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_service.h"
#include "components/tabs/public/tab_interface.h"

namespace skills {

DEFINE_USER_DATA(SkillsUiWindowController);

SkillsUiWindowController::SkillsUiWindowController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_data_holder_(browser_window_interface->GetUnownedUserDataHost(),
                          *this) {}

SkillsUiWindowController::~SkillsUiWindowController() {
  if (!pending_deletions_.empty()) {
    skills::SkillsService* skills_service = SkillsServiceFactory::GetForProfile(
        browser_window_interface_->GetProfile());
    if (skills_service) {
      for (const std::string& skill_id : pending_deletions_) {
        skills_service->DeleteSkill(skill_id,
                                    SkillsService::UpdateSource::kLocal);
      }
    }
  }
}

SkillsUiWindowController* SkillsUiWindowController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

void SkillsUiWindowController::OnSkillSaved(std::string_view skill_id,
                                            bool hide_toast_button) {
  last_saved_skill_id_ = skill_id;
  ShowToast(hide_toast_button ? ToastId::kSkillSavedWithoutInvokeButton
                              : ToastId::kSkillSaved);
}

void SkillsUiWindowController::OnSkillDeleted(std::string_view skill_id) {
  last_deleted_skill_id_ = skill_id;
  pending_deletions_.insert(last_deleted_skill_id_);
  skills::SkillsService* skills_service = SkillsServiceFactory::GetForProfile(
      browser_window_interface_->GetProfile());
  if (skills_service) {
    skills_service->NotifyTemporarySkillDisplayChanged(
        last_deleted_skill_id_, skills::SkillsService::DisplayState::kDeleted);
  }
  ToastParams params(ToastId::kSkillDeleted);
  params.toast_close_callback = base::ScopedClosureRunner(
      base::BindOnce(&SkillsUiWindowController::OnToastClosed,
                     weak_factory_.GetWeakPtr(), last_deleted_skill_id_));
  ShowSkillToast(std::move(params));
}

void SkillsUiWindowController::ShowToast(
    ToastId toast_id,
    const std::string& skill_id,
    base::OnceCallback<void(bool)> callback) {
  action_clicked_ = false;

  ToastParams params(toast_id);
  // Deleted toasts are only triggered here from skillsV2. Other paths follow
  // OnSkillDeleted.
  if (toast_id == ToastId::kSkillDeleted) {
    if (callback) {
      skills_v2_delete_callbacks_[skill_id] = std::move(callback);
    }
    // Add a closed callback for the deleted toast.
    params.toast_close_callback = base::ScopedClosureRunner(
        base::BindOnce(&SkillsUiWindowController::OnToastClosed,
                       weak_factory_.GetWeakPtr(), skill_id));
  }
  ShowSkillToast(std::move(params));
}

void SkillsUiWindowController::UndoLastSkillRemoval() {
  action_clicked_ = true;
  if (last_deleted_skill_id_.empty()) {
    return;
  }

  // If the skill is in pending deletions, remove it so it won't be deleted.
  if (pending_deletions_.contains(last_deleted_skill_id_)) {
    pending_deletions_.erase(last_deleted_skill_id_);

    // Notify the UI to reshow the skill.
    skills::SkillsService* skills_service = SkillsServiceFactory::GetForProfile(
        browser_window_interface_->GetProfile());
    if (skills_service) {
      skills_service->NotifyTemporarySkillDisplayChanged(
          last_deleted_skill_id_,
          skills::SkillsService::DisplayState::kReshown);
    }
  }

  last_deleted_skill_id_.clear();
}

void SkillsUiWindowController::OnToastClosed(const std::string& skill_id) {
  auto it = skills_v2_delete_callbacks_.find(skill_id);
  if (it != skills_v2_delete_callbacks_.end()) {
    std::move(it->second).Run(action_clicked_);
    skills_v2_delete_callbacks_.erase(it);
    action_clicked_ = false;
    return;
  }

  // Only delete if the skill is still in the pending set
  if (pending_deletions_.contains(skill_id)) {
    skills::SkillsService* skills_service = SkillsServiceFactory::GetForProfile(
        browser_window_interface_->GetProfile());
    if (skills_service) {
      skills_service->DeleteSkill(skill_id,
                                  SkillsService::UpdateSource::kLocal);
    }
    pending_deletions_.erase(skill_id);
  }
}

void SkillsUiWindowController::ShowSkillToast(ToastParams params) {
  browser_window_interface_->GetFeatures().toast_controller()->MaybeShowToast(
      std::move(params));
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
      tab_controller->InvokeSkill(skill_id, /*skill_name=*/"",
                                  /*skill_icon=*/"");
    }
  }
}

}  // namespace skills
