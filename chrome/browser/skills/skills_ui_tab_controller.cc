// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_ui_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/skills/skills_dialog.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

DEFINE_USER_DATA(skills::SkillsUiTabController);

namespace skills {

SkillsUiTabController::SkillsUiTabController(tabs::TabInterface& tab)
    : SkillsUiTabControllerInterface(tab),
      tab_(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

SkillsUiTabController::~SkillsUiTabController() = default;

void SkillsUiTabController::ShowDialog() {
  if (dialog_delegate_) {
    return;
  }
  content::WebContents* contents = tab_->GetContents();
  CHECK(contents);
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  // TODO(crbug.com/476145843): Pass in the prompt and a weak pointer to the tab
  // controller in the dialog.
  auto delegate = std::make_unique<SkillsDialog>(profile);
  delegate->RegisterOnDialogClosedCallback(base::BindOnce(
      &SkillsUiTabController::OnDialogClosed, weak_ptr_factory_.GetWeakPtr()));

  dialog_delegate_ =
      ShowConstrainedWebDialog(profile, std::move(delegate), contents);
}

void SkillsUiTabController::CloseDialog() {
  if (dialog_delegate_) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        dialog_delegate_->GetNativeDialog());
    if (widget && !widget->IsClosed()) {
      widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    }
    return;
  }
}

void SkillsUiTabController::OnDialogClosed(const std::string& json_retval) {
  dialog_delegate_ = nullptr;
  if (on_dialog_closed_callback_for_testing_) {
    std::move(on_dialog_closed_callback_for_testing_).Run();
  }
}

void SkillsUiTabController::OnSkillSaved(const std::string& skill_id) {
  if (auto* window_interface = tab_->GetBrowserWindowInterface()) {
    // Delegate the global toast action to the Window Controller.
    auto* window_controller = SkillsUiController::From(window_interface);
    if (window_controller) {
      window_controller->OnSkillSaved(skill_id);
    }
  }
}

}  // namespace skills
