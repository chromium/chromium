// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_tab_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_ui_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/browser/ui/webui/skills/skills_dialog.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

DEFINE_USER_DATA(skills::SkillsUiTabController);

namespace {
using glic::mojom::SkillSource;

constexpr base::TimeDelta kNotifyTimeoutSeconds = base::Seconds(60);
constexpr base::TimeDelta kGlicPanelPollIntervalMilliseconds =
    base::Milliseconds(60);

#if BUILDFLAG(ENABLE_GLIC)
glic::mojom::SkillPreviewPtr GetPreviewFromSkill(const skills::Skill& skill) {
  auto skill_preview = glic::mojom::SkillPreview::New();
  skill_preview->id = skill.id;
  skill_preview->name = skill.name;
  skill_preview->icon = skill.icon;

  switch (skill.source) {
    case skills::SkillSource::kFirstParty:
      skill_preview->source = SkillSource::kFirstParty;
      break;
    case skills::SkillSource::kUserCreated:
      skill_preview->source = SkillSource::kUserCreated;
      break;
    default:
      skill_preview->source = SkillSource::kUnknown;
  }
  return skill_preview;
}
#endif  // BUILDFLAG(ENABLE_GLIC)

}  // namespace

namespace skills {

SkillsUiTabController::SkillsUiTabController(tabs::TabInterface& tab)
    : SkillsUiTabControllerInterface(tab),
      tab_(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

SkillsUiTabController::~SkillsUiTabController() = default;

void SkillsUiTabController::ShowDialog(const skills::Skill& skill) {
  if (dialog_delegate_) {
    return;
  }
  content::WebContents* contents = tab_->GetContents();
  CHECK(contents);
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  // TODO(crbug.com/476145843): Pass in the skill and a weak pointer to the tab
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

void SkillsUiTabController::InvokeSkill(std::string_view skill_id) {
  if (pending_skill_id_.empty()) {
    ShowGlicPanel();
  }

  pending_skill_id_ = skill_id;

  glic_panel_open_time_ = base::TimeTicks::Now();

  NotifySkillToInvokeChangedWhenReady();
}

glic::GlicKeyedService* SkillsUiTabController::GetGlicService() {
#if BUILDFLAG(ENABLE_GLIC)
  content::WebContents* contents = tab_->GetContents();
  if (!contents) {
    return nullptr;
  }
  return glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      contents->GetBrowserContext());
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void SkillsUiTabController::ShowGlicPanel() {
#if BUILDFLAG(ENABLE_GLIC)
  if (auto* service = GetGlicService()) {
    service->ToggleUI(tab_->GetBrowserWindowInterface(),
                      /*prevent_close=*/true,
                      glic::mojom::InvocationSource::kSkills);
  }
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void SkillsUiTabController::NotifySkillToInvokeChangedWhenReady() {
  if (IsClientReady()) {
    // TODO(https://crbug.com/475549806): Add metrics for successful skill
    // invocation.
    NotifySkillToInvokeChanged();
  } else if (base::TimeTicks::Now() - glic_panel_open_time_ >
             kNotifyTimeoutSeconds) {
    // TODO(https://crbug.com/475549806): Add metrics for skill invocation
    // timeout and provide user feedback.
    Reset();
  } else if (!glic_panel_ready_timer_.IsRunning()) {
    glic_panel_ready_timer_.Start(
        FROM_HERE, kGlicPanelPollIntervalMilliseconds,
        base::BindRepeating(
            &SkillsUiTabController::NotifySkillToInvokeChangedWhenReady,
            base::Unretained(this)));
  }
}

void SkillsUiTabController::NotifySkillToInvokeChanged() {
  std::string skill_id_to_invoke = pending_skill_id_;

  Reset();
  CHECK(!glic_panel_ready_timer_.IsRunning());

  content::WebContents* contents = tab_->GetContents();
  if (!contents) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  skills::SkillsService* skills_service =
      skills::SkillsServiceFactory::GetForProfile(profile);

  if (!skills_service) {
    return;
  }

  const skills::Skill* skill = skills_service->GetSkillById(skill_id_to_invoke);

  if (!skill) {
    // TODO(https://crbug.com/475549806): Add metrics for skill invocation
    // failure and provide user feedback.
    return;
  }

#if BUILDFLAG(ENABLE_GLIC)
  auto mojo_skill = glic::mojom::Skill::New();
  mojo_skill->prompt = skill->prompt;
  mojo_skill->preview = GetPreviewFromSkill(*skill);

  if (auto* service = GetGlicService()) {
    if (auto* instance = service->GetInstanceForTab(&tab_.get())) {
      instance->host().NotifySkillToInvokeChanged(std::move(mojo_skill));
    }
  }
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void SkillsUiTabController::Reset() {
  glic_panel_open_time_ = base::TimeTicks();
  glic_panel_ready_timer_.Stop();
  pending_skill_id_ = "";
}

bool SkillsUiTabController::IsClientReady() {
#if BUILDFLAG(ENABLE_GLIC)
  if (auto* service = GetGlicService()) {
    if (auto* instance = service->GetInstanceForTab(&tab_.get())) {
      return instance->host().IsReady();
    }
  }
  return false;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_GLIC)
}

}  // namespace skills
