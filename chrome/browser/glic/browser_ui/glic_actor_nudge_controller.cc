// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_actor_nudge_controller.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/browser_ui/glic_actor_nudge_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

using actor::ui::ActorTaskNudgeState;
using glic::GlicInstance;
using glic::GlicKeyedService;
using glic::Host;

DEFINE_USER_DATA(GlicActorNudgeController);

GlicActorNudgeController::GlicActorNudgeController(
    BrowserWindowInterface* browser,
    GlicActorNudgeDelegate* horizontal_tabs_delegate,
    GlicActorNudgeDelegate* vertical_tabs_delegate)
    : profile_(browser->GetProfile()),
      browser_(browser),
      horizontal_tabs_delegate_(horizontal_tabs_delegate),
      vertical_tabs_delegate_(vertical_tabs_delegate),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    RegisterActorNudgeStateCallback();
  }

  ActorTaskListBubbleController* bubble_controller =
      ActorTaskListBubbleController::From(browser_);
  bubble_visibility_change_subscription_.push_back(
      bubble_controller->RegisterBubbleShownCallback(base::BindRepeating(
          &GlicActorNudgeController::OnBubbleVisibilityChange,
          weak_ptr_factory_.GetWeakPtr(), /*is_bubble_open=*/true)));
  bubble_visibility_change_subscription_.push_back(
      bubble_controller->RegisterBubbleDestroyedCallback(base::BindRepeating(
          &GlicActorNudgeController::OnBubbleVisibilityChange,
          weak_ptr_factory_.GetWeakPtr(), /*is_bubble_open=*/false)));
}

GlicActorNudgeController::~GlicActorNudgeController() = default;

// static
GlicActorNudgeController* GlicActorNudgeController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

void GlicActorNudgeController::OnStateUpdate(
    bool show_bubble,
    ActorTaskNudgeState actor_task_nudge_state) {
  // If the task icon is inactive, hide it and perform no additional style
  // changes.
  GlicActorTaskIconManager* manager =
      GlicActorTaskIconManagerFactory::GetForProfile(profile_);
  DCHECK(manager);
  if (manager->actor_task_list_bubble_rows().empty()) {
    HideGlicActorTaskIcon();
    CloseBubble();
    return;
  }

  size_t num_tasks_need_processing = manager->GetNumActorTasksNeedProcessing();
  switch (actor_task_nudge_state.text) {
    case ActorTaskNudgeState::Text::kDefault:
      ShowGlicActorTaskIcon();
      if (show_bubble) {
        ShowBubble();
      } else {
        // In either case, close the bubble as the nudge has been either hidden
        // or reset.
        CloseBubble();
      }
      break;
    case ActorTaskNudgeState::Text::kNeedsAttention:
      UpdateNudgeLabelOrRetrigger(
          l10n_util::GetPluralStringFUTF16(
              IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, num_tasks_need_processing),
          show_bubble);
      break;
    case ActorTaskNudgeState::Text::kCompleteTasks:
      UpdateNudgeLabelOrRetrigger(l10n_util::GetPluralStringFUTF16(
                                      IDS_ACTOR_TASK_NUDGE_TASK_COMPLETE_LABEL,
                                      actor::ActorKeyedService::Get(profile_)
                                          ->GetActorUiStateManager()
                                          ->GetInactiveTaskCount()),
                                  show_bubble);
      break;
    default:
      NOTREACHED();
  }

  if (IsShowingNudge()) {
    actor::ui::RecordGlobalTaskIndicatorNudgeShown(actor_task_nudge_state);
  }
}

void GlicActorNudgeController::UpdateNudgeLabelOrRetrigger(
    std::u16string nudge_label_text,
    bool show_bubble) {
  if (IsShowingNudge()) {
    SetGlicActorNudgeLabel(nudge_label_text);
  } else {
    TriggerGlicActorNudge(nudge_label_text);
  }

  if (show_bubble) {
    ShowBubble();
  }
}

void GlicActorNudgeController::RegisterActorNudgeStateCallback() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    actor_nudge_state_change_callback_subscription_.push_back(
        manager->RegisterTaskNudgeStateChange(base::BindRepeating(
            &GlicActorNudgeController::OnStateUpdate, base::Unretained(this))));
  }
}

void GlicActorNudgeController::UpdateCurrentActorNudgeState() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    // This will "sync" a new window's state to the current nudge state. Do not
    // show the bubble in the new window as the user navigated away from the
    // bubble that was previously shown.
    OnStateUpdate(/*show_bubble=*/false,
                  manager->GetCurrentActorTaskNudgeState());
  }
}

void GlicActorNudgeController::ShowGlicActorTaskIcon() {
  CallOnBoth(base::BindRepeating([](GlicActorNudgeDelegate& delegate) {
    delegate.ShowGlicActorTaskIcon();
  }));
}

void GlicActorNudgeController::HideGlicActorTaskIcon() {
  CallOnBoth(base::BindRepeating([](GlicActorNudgeDelegate& delegate) {
    delegate.HideGlicActorTaskIcon();
  }));
}

void GlicActorNudgeController::SetGlicActorNudgeLabel(
    const std::u16string& nudge_label) {
  CallOnBoth(base::BindRepeating(
      [](const std::u16string& nudge_label, GlicActorNudgeDelegate& delegate) {
        delegate.SetGlicActorNudgeLabel(nudge_label);
      },
      nudge_label));
}

void GlicActorNudgeController::TriggerGlicActorNudge(
    const std::u16string& nudge_text) {
  CallOnBoth(base::BindRepeating(
      [](const std::u16string& nudge_text, GlicActorNudgeDelegate& delegate) {
        delegate.TriggerGlicActorNudge(nudge_text);
      },
      nudge_text));
}

void GlicActorNudgeController::ShowBubble() {
  if (auto* delegate = GetActiveDelegate()) {
    delegate->ShowActorTaskListBubble();
  }
}

void GlicActorNudgeController::CloseBubble() {
  ActorTaskListBubbleController* bubble_controller =
      ActorTaskListBubbleController::From(browser_);
  if (bubble_controller->GetBubbleWidget()) {
    bubble_controller->GetBubbleWidget()->Close();
  }
}

bool GlicActorNudgeController::IsShowingNudge() {
  return (IsDelegateActive(horizontal_tabs_delegate_) &&
          horizontal_tabs_delegate_->GetIsShowingGlicActorTaskIconNudge()) ||
         (IsDelegateActive(vertical_tabs_delegate_) &&
          vertical_tabs_delegate_->GetIsShowingGlicActorTaskIconNudge());
}

void GlicActorNudgeController::OnBubbleVisibilityChange(bool is_bubble_open) {
  CallOnBoth(base::BindRepeating(
      [](bool is_bubble_open, GlicActorNudgeDelegate& delegate) {
        delegate.SetGlicActorNudgePressedState(is_bubble_open);
      },
      is_bubble_open));
}

void GlicActorNudgeController::CallOnBoth(
    base::RepeatingCallback<void(GlicActorNudgeDelegate&)> fn) {
  // One or both or neither delegate may need updated.
  if (IsDelegateActive(horizontal_tabs_delegate_)) {
    fn.Run(*horizontal_tabs_delegate_);
  }
  if (IsDelegateActive(vertical_tabs_delegate_)) {
    fn.Run(*vertical_tabs_delegate_);
  }
}

bool GlicActorNudgeController::IsDelegateActive(
    GlicActorNudgeDelegate* delegate) const {
  return delegate && delegate->IsGlicAdded();
}

GlicActorNudgeDelegate* GlicActorNudgeController::GetActiveDelegate() const {
  auto* vertical_tab_strip_state_controller =
      tabs::VerticalTabStripStateController::From(browser_);
  if (vertical_tab_strip_state_controller->ShouldDisplayVerticalTabs() &&
      IsDelegateActive(vertical_tabs_delegate_)) {
    return vertical_tabs_delegate_;
  }
  if (IsDelegateActive(horizontal_tabs_delegate_)) {
    return horizontal_tabs_delegate_;
  }
  return nullptr;
}

}  // namespace glic
