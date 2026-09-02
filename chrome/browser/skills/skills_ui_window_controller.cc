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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_prefs.h"
#include "components/skills/public/skills_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace skills {

DEFINE_USER_DATA(SkillsUiWindowController);

SkillsUiWindowController::SkillsUiWindowController(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      scoped_data_holder_(browser_window_interface->GetUnownedUserDataHost(),
                          *this) {
  Profile* profile = browser_window_interface_->GetProfile();
  if (profile) {
    pref_registrar_.Init(profile->GetPrefs());
    pref_registrar_.Add(
        skills::prefs::kChromeSkillsEnabled,
        base::BindRepeating(
            &SkillsUiWindowController::CloseDialogsAndReloadSkillsPages,
            weak_factory_.GetWeakPtr()));
  }
}

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

void SkillsUiWindowController::CloseDialogsAndReloadSkillsPages() {
  TabStripModel* tab_strip_model =
      browser_window_interface_->GetTabStripModel();
  if (!tab_strip_model) {
    return;
  }

  for (int i = 0; i < tab_strip_model->count(); ++i) {
    tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(i);
    if (!tab) {
      continue;
    }

    if (auto* tab_controller = SkillsUiTabControllerInterface::From(tab)) {
      if (tab_controller->IsShowing()) {
        tab_controller->CloseDialog();
      }
    }

    content::WebContents* web_contents = tab->GetContents();
    if (web_contents &&
        web_contents->GetVisibleURL().host() == chrome::kChromeUISkillsHost) {
      web_contents->GetController().Reload(content::ReloadType::NORMAL,
                                           /*check_for_repost=*/false);
    }
  }
}

void SkillsUiWindowController::OnSkillSaved(std::string_view skill_id,
                                            bool hide_toast_button) {
  StoreLastSavedSkillMetadata(skill_id, "", "");
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
  ToastController::From(browser_window_interface_)
      ->MaybeShowToast(std::move(params));
}

void SkillsUiWindowController::InvokeLastSavedSkill() {
  InvokeSkill(last_saved_skill_id_, last_saved_skill_name_,
              last_saved_skill_icon_);
}

void SkillsUiWindowController::InvokeSkill(std::string_view skill_id,
                                           std::string_view skill_name,
                                           std::string_view skill_icon) {
  if (skill_id.empty()) {
    return;
  }
  if (!SkillsServiceFactory::IsSkillsEnabledForProfile(
          browser_window_interface_->GetProfile())) {
    return;
  }

  if (auto* active_tab = browser_window_interface_->GetActiveTabInterface()) {
    if (auto* tab_controller =
            skills::SkillsUiTabControllerInterface::From(active_tab)) {
      tab_controller->InvokeSkill(skill_id, skill_name, skill_icon,
                                  /*auto_submit=*/true);
    }
  }
}

void SkillsUiWindowController::StoreLastSavedSkillMetadata(
    std::string_view skill_id,
    std::string_view skill_name,
    std::string_view skill_icon) {
  last_saved_skill_id_ = skill_id;
  last_saved_skill_name_ = skill_name;
  last_saved_skill_icon_ = skill_icon;
}

}  // namespace skills
