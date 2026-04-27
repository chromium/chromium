// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
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

constexpr base::TimeDelta kNotifyTimeoutSeconds = base::Seconds(60);
constexpr base::TimeDelta kGlicPanelPollIntervalMilliseconds =
    base::Milliseconds(60);

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
                                       mojom::SkillsDialogType dialog_type) {
  if (dialog_widget_) {
    // Dialog is already open.
    return;
  }
  // TODO(crbug.com/477385216): Update to use an enum for creation mode.
  RecordSkillsDialogAction(SkillsDialogAction::kOpened, entrypoint,
                           /*is_edit_mode=*/IsEditMode(&skill));
  current_skill_ = skill;

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
  if (pending_skill_id_.empty()) {
    ShowGlicPanel();
  }

  pending_skill_id_ = skill_id;

  glic_panel_open_time_ = base::TimeTicks::Now();

  NotifySkillToInvokeChangedWhenReady();
}

glic::GlicKeyedService* SkillsUiTabController::GetGlicService() {
  content::WebContents* contents = tab_->GetContents();
  if (!contents) {
    return nullptr;
  }
  return glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      contents->GetBrowserContext());
}

void SkillsUiTabController::ShowGlicPanel() {
  if (auto* service = GetGlicService()) {
    service->ToggleUI(tab_->GetBrowserWindowInterface(),
                      /*prevent_close=*/true,
                      glic::mojom::InvocationSource::kSkills);
  }
}

void SkillsUiTabController::NotifySkillToInvokeChangedWhenReady() {
  // TODO(b/483387751): refactor to use invoke API.
  if (IsClientReady()) {
    NotifySkillToInvokeChanged();
  } else if (base::TimeTicks::Now() - glic_panel_open_time_ >
             kNotifyTimeoutSeconds) {
    RecordSkillsInvokeResult(SkillsInvokeResult::kTimeout);
    Reset();
  } else if (!glic_panel_ready_timer_.IsRunning()) {
    glic_panel_ready_timer_.Start(
        FROM_HERE, kGlicPanelPollIntervalMilliseconds,
        base::BindRepeating(
            &SkillsUiTabController::NotifySkillToInvokeChangedWhenReady,
            base::Unretained(this)));
  }
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

void SkillsUiTabController::NotifySkillToInvokeChanged() {
  std::string skill_id_to_invoke = pending_skill_id_;

  Reset();
  CHECK(!glic_panel_ready_timer_.IsRunning());

  const skills::Skill* skill = GetSkill(skill_id_to_invoke);

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

  auto mojo_skill = glic::mojom::Skill::New();
  mojo_skill->prompt = skill->prompt;
  mojo_skill->preview = SkillToGlicMojomSkillPreview(skill);

  if (auto* service = GetGlicService()) {
    if (auto* instance = service->GetInstanceForTab(&tab_.get())) {
      instance->host().NotifySkillToInvokeChanged(std::move(mojo_skill));
    }
  }
}

void SkillsUiTabController::Reset() {
  glic_panel_open_time_ = base::TimeTicks();
  glic_panel_ready_timer_.Stop();
  pending_skill_id_ = "";
}

bool SkillsUiTabController::IsClientReady() {
  if (auto* service = GetGlicService()) {
    if (auto* instance = service->GetInstanceForTab(&tab_.get())) {
      return instance->host().IsWebClientConnected();
    }
  }
  return false;
}

}  // namespace skills
