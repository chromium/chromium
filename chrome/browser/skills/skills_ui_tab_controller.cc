// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_glic_mojom_util.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_delegate.h"
#include "chrome/browser/ui/webui/skills/skills_dialog_view.h"
#include "chrome/browser/ui/webui/skills/skills_ui.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/window/dialog_delegate.h"

DEFINE_USER_DATA(skills::SkillsUiTabController);

namespace {

using glic::mojom::SkillSource;

}  // namespace

namespace skills {

SkillsUiTabController::SkillsUiTabController(tabs::TabInterface& tab)
    : SkillsUiTabControllerInterface(tab),
      tab_(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  will_detach_subscription_ = tab.RegisterWillDetach(base::BindRepeating(
      &SkillsUiTabController::OnTabWillDetach, base::Unretained(this)));
}

SkillsUiTabController::~SkillsUiTabController() {
  if (dialog_widget_) {
    dialog_widget_->RemoveObserver(this);
    OnDialogClosing(views::Widget::ClosedReason::kUnspecified);
  }
}

void SkillsUiTabController::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  // Synchronously close the widget on 'kDelete' while the tab's UI
  // scaffolding is still valid. This prevents re-entrancy crashes during tab
  // closure.
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    OnDialogClosing(views::Widget::ClosedReason::kUnspecified);
  }
}

void SkillsUiTabController::ShowDialog(Skill skill,
                                       SkillsDialogEntryPoint entrypoint,
                                       mojom::SkillsDialogType dialog_type,
                                       std::unique_ptr<glic::Target> target) {
  if (dialog_widget_) {
    // Dialog is already open.
    return;
  }
  // TODO(crbug.com/477385216): Update to use an enum for creation mode.
  RecordSkillsDialogAction(SkillsDialogAction::kOpened, entrypoint,
                           /*is_edit_mode=*/IsEditMode(&skill));
  current_skill_ = skill;
  target_ = std::move(target);

  content::WebContents* contents = tab_->GetContents();
  CHECK(contents);
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto dialog_view = std::make_unique<skills::SkillsDialogView>(profile);

  dialog_delegate_ = std::make_unique<views::DialogDelegate>();
  dialog_delegate_->SetShowCloseButton(false);
  dialog_delegate_->SetButtons(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  dialog_delegate_->SetModalType(ui::mojom::ModalType::kChild);
  dialog_delegate_->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  // Create Skills Dialog Delegate.
  content::WebContents* dialog_contents = dialog_view->web_contents();
  if (dialog_contents && dialog_contents->GetWebUI()) {
    if (auto* skills_ui = dialog_contents->GetWebUI()
                              ->GetController()
                              ->GetAs<skills::SkillsUI>()) {
      skills_ui->InitializeDialog(weak_ptr_factory_.GetWeakPtr(),
                                  std::move(skill), entrypoint, dialog_type);
    }
  }
  dialog_delegate_->SetInitiallyFocusedView(dialog_view->web_view());
  dialog_delegate_->SetContentsView(std::move(dialog_view));
  dialog_widget_ = constrained_window::ShowWebModalDialogViewsOwned(
      dialog_delegate_.get(), tab_->GetContents(),
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);

  dialog_widget_->MakeCloseSynchronous(base::BindOnce(
      &SkillsUiTabController::OnDialogClosing, weak_ptr_factory_.GetWeakPtr()));
}

void SkillsUiTabController::OnDialogClosing(
    views::Widget::ClosedReason reason) {
  if (!dialog_widget_) {
    return;
  }
  dialog_widget_.reset();
  dialog_delegate_.reset();
  if (on_dialog_closed_callback_for_testing_) {
    std::move(on_dialog_closed_callback_for_testing_).Run();
  }
}

void SkillsUiTabController::CloseDialog() {
  if (!dialog_widget_) {
    return;
  }
  dialog_widget_->Close();
}

void SkillsUiTabController::OnWidgetDestroyed(views::Widget* widget) {
  if (dialog_widget_.get() != widget) {
    return;
  }
  // Call the central closing logic to ensure state is reset.
  OnDialogClosing(views::Widget::ClosedReason::kUnspecified);
}

void SkillsUiTabController::OnSkillSaved(const std::string& skill_id) {
  if (auto* window_interface = tab_->GetBrowserWindowInterface()) {
    // Delegate the global toast action to the Window Controller.
    auto* window_controller = SkillsUiWindowController::From(window_interface);
    if (window_controller) {
      bool hide_toast_button =
          tab_->GetContents()->GetVisibleURL().spec().starts_with(
              chrome::kChromeUISkillsURL);
      window_controller->OnSkillSaved(skill_id, hide_toast_button);
    }
  }
}

void SkillsUiTabController::OnSkillDeleted(const std::string& skill_id) {
  if (auto* window_interface = tab_->GetBrowserWindowInterface()) {
    // Delegate the global toast action to the Window Controller.
    auto* window_controller = SkillsUiWindowController::From(window_interface);
    if (window_controller) {
      window_controller->OnSkillDeleted(skill_id);
    }
  }
}

bool SkillsUiTabController::IsShowing() const {
  return dialog_widget_ != nullptr;
}

void SkillsUiTabController::InvokeSkill(std::string_view skill_id) {
  last_invoked_skill_id_for_testing_ = skill_id;
  const skills::Skill* skill = GetSkill(skill_id);

  if (!skill) {
    // TODO(https://crbug.com/475549806): provide user feedback.
    RecordSkillsInvokeResult(SkillsInvokeResult::kSkillNotFound);
    return;
  }

  RecordSkillsInvokeResult(SkillsInvokeResult::kSuccess);
  switch (skill->source) {
    case sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY:
      RecordSkillsInvokeAction(SkillsInvokeAction::kFirstParty);
      break;
    case sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED:
      RecordSkillsInvokeAction(SkillsInvokeAction::kUserCreated);
      break;
    case sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY:
      RecordSkillsInvokeAction(SkillsInvokeAction::kDerivedFromFirstParty);
      break;
    // This is an edge case. It occurs when there is an update that introduces
    // a new SkillSource, but the user is using an older version of Chrome that
    // isn't updated to support the new SkillSource.
    case sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN:
      RecordSkillsInvokeAction(SkillsInvokeAction::kUnknown);
      break;
  }

  if (auto* service = GetGlicService()) {
    glic::GlicInvokeOptions options(
        glic::Target(tab_.get(), glic::DefaultConversation()),
        glic::mojom::InvocationSource::kSkills);
    options.prompts.push_back(skill->prompt);
    options.skill_id = std::string(skill_id);
    if (target_) {
      options.target = std::move(*target_);
      target_.reset();
    }
    service->InvokeWithAutoSubmit(
        glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
        std::move(options));
  }
}

glic::GlicKeyedService* SkillsUiTabController::GetGlicService() {
  content::WebContents* contents = tab_->GetContents();
  if (!contents) {
    return nullptr;
  }
  return glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      contents->GetBrowserContext());
}

const skills::Skill* SkillsUiTabController::GetSkill(
    std::string_view skill_id) {
  content::WebContents* contents = tab_->GetContents();
  if (!contents) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  auto* service = skills::SkillsServiceFactory::GetForProfile(profile);
  return service ? service->GetSkillById(skill_id) : nullptr;
}

}  // namespace skills
