// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_dialog_launcher.h"

#include "chrome/browser/skills/skills_ui_tab_controller_interface.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom-forward.h"
#include "components/skills/public/skills_metrics.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace skills {

WEB_CONTENTS_USER_DATA_KEY_IMPL(SkillsDialogLauncher);

// static
void SkillsDialogLauncher::TriggerDialog(tabs::TabInterface* tab,
                                         Skill skill,
                                         mojom::SkillsDialogType dialog_type,
                                         SkillResultCallback callback) {
  if (!tab) {
    return;
  }

  if (auto* controller = SkillsUiTabControllerInterface::From(tab)) {
    SkillsDialogEntryPoint entrypoint = ResolveEntryPointForWebClient(&skill);
    controller->ShowDialog(std::move(skill), entrypoint, dialog_type);
    std::move(callback).Run(true);
  }
}

// static
void SkillsDialogLauncher::CreateForTab(tabs::TabInterface* tab,
                                        Skill skill,
                                        mojom::SkillsDialogType dialog_type,
                                        SkillResultCallback callback) {
  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return;
  }
  // Page is already loaded, show immediately.
  if (!contents->IsLoading()) {
    TriggerDialog(tab, std::move(skill), dialog_type, std::move(callback));
    return;
  }
  // If there is already a request in flight, return false to indicate failure.
  if (FromWebContents(contents)) {
    std::move(callback).Run(false);
    return;
  }

  // CreateForWebContents will attach the object to the tab.
  // If one already exists, it does nothing (prevents double-launch).
  CreateForWebContents(contents, tab, std::move(skill), std::move(dialog_type),
                       std::move(callback));
}

SkillsDialogLauncher::SkillsDialogLauncher(content::WebContents* contents,
                                           tabs::TabInterface* tab,
                                           Skill skill,
                                           mojom::SkillsDialogType dialog_type,
                                           SkillResultCallback callback)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<SkillsDialogLauncher>(*contents),
      tab_(tab->GetWeakPtr()),
      skill_(std::move(skill)),
      dialog_type_(dialog_type),
      callback_(std::move(callback)) {}

SkillsDialogLauncher::~SkillsDialogLauncher() = default;

void SkillsDialogLauncher::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  // Once any navigation commits or fails in the main frame, we
  // attempt to show the dialog so the user isn't left waiting.
  Show();
}

void SkillsDialogLauncher::Show() {
  if (tab_ && tab_->GetContents() == web_contents()) {
    TriggerDialog(tab_.get(), std::move(skill_), std::move(dialog_type_),
                  std::move(callback_));
  } else if (callback_) {
    std::move(callback_).Run(false);
  }
  web_contents()->RemoveUserData(UserDataKey());
}

}  // namespace skills
