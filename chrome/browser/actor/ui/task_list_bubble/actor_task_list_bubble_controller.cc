// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"

#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_USER_DATA(ActorTaskListBubbleController);

ActorTaskListBubbleController::ActorTaskListBubbleController(
    BrowserWindowInterface* browser_window,
    glic::GlicSplitButtonController& split_button_controller)
    : browser_(browser_window),
      split_button_controller_(split_button_controller),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  auto* manager = glic::GlicActorTaskIconManagerFactory::GetForProfile(
      browser_->GetProfile());
  DCHECK(manager);
  bubble_state_change_callback_subscription_.push_back(
      manager->RegisterTaskListBubbleStateChange(
          base::BindRepeating(&ActorTaskListBubbleController::OnStateUpdate,
                              base::Unretained(this))));
}

ActorTaskListBubbleController::~ActorTaskListBubbleController() = default;

void ActorTaskListBubbleController::ShowBubble(bool is_start_notification) {
  const bool should_delay = !IsBubbleShowing() && is_start_notification &&
                            base::FeatureList::IsEnabled(
                                features::kGlicActorUiTaskListBubbleDelayShow);

  if (should_delay) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ActorTaskListBubbleController::ShowBubbleImpl,
                       weak_ptr_factory_.GetWeakPtr(), is_start_notification),
        base::Milliseconds(features::kGlicActorUiTaskListBubbleDelayMs.Get()));
  } else {
    ShowBubbleImpl(is_start_notification);
  }
}

void ActorTaskListBubbleController::CloseBubble() {
  if (auto* delegate = split_button_controller_->GetActiveDelegate()) {
    delegate->CloseActorTaskListBubble();
  }
}

bool ActorTaskListBubbleController::IsBubbleShowing() const {
  auto* delegate = GetActiveDelegate();
  return delegate && delegate->IsActorTaskListBubbleShowing();
}

void ActorTaskListBubbleController::ShowBubbleImpl(bool is_start_notification) {
  auto* delegate = GetActiveDelegate();
  if (!delegate) {
    return;
  }

  auto* manager = glic::GlicActorTaskIconManagerFactory::GetForProfile(
      browser_->GetProfile());
  DCHECK(manager);

  // If the browser is in the background, only show the bubble if this is a
  // start notification for an experimentalTriggering task triggered while
  // the Glic panel is visible on this window. We avoid popping up the bubble
  // for subsequent background task status updates to avoid disturbing the user.
  if (!browser_->IsActive()) {
    auto* glic_service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        browser_->GetProfile());
    if (!is_start_notification || !glic_service ||
        !glic_service->IsPanelShowingForBrowser(*browser_)) {
      return;
    }
  }

  const auto& task_id_to_state = manager->actor_task_list_bubble_rows();
  // Do not show bubble if there are no rows to show.
  if (task_id_to_state.empty()) {
    return;
  }
  // Close any existing bubble widget to avoid stacking multiple bubble windows.
  split_button_controller_->CallOnBoth(
      base::BindRepeating([](glic::GlicSplitButtonDelegate& delegate) {
        delegate.CloseActorTaskListBubble();
      }));
  delegate->ShowActorTaskListBubble();

  // All rows may be skipped, in which case the bubble will not be shown.
  if (delegate->IsActorTaskListBubbleShowing()) {
    on_bubble_shown_callback_list.Notify();
  }
}

void ActorTaskListBubbleController::OnStateUpdate(bool is_start_notification) {
  auto* delegate = GetActiveDelegate();
  if (!delegate) {
    return;
  }

  if (delegate->IsActorTaskListBubbleShowing() || is_start_notification) {
    ShowBubble(is_start_notification);
  }
}

void ActorTaskListBubbleController::OnBubbleDestroyed() {
  on_bubble_destroyed_callback_list.Notify();
}

base::CallbackListSubscription
ActorTaskListBubbleController::RegisterBubbleShownCallback(
    base::RepeatingClosure callback) {
  return on_bubble_shown_callback_list.Add(std::move(callback));
}

base::CallbackListSubscription
ActorTaskListBubbleController::RegisterBubbleDestroyedCallback(
    base::RepeatingClosure callback) {
  return on_bubble_destroyed_callback_list.Add(std::move(callback));
}

void ActorTaskListBubbleController::OnTaskRowClicked(actor::TaskId task_id) {
  Profile* profile = browser_->GetProfile();
  actor::ui::ActorUiStateManagerInterface* manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  if (auto last_tab_opt = manager->GetLastActedOnTab(task_id);
      last_tab_opt && *last_tab_opt) {
    tabs::TabInterface* last_tab = *last_tab_opt;
    TabListInterface::From(last_tab->GetBrowserWindowInterface())
        ->ActivateTab(last_tab->GetHandle());
    // Activate the window that the tab is in as it may not be the current one.
    last_tab->GetBrowserWindowInterface()->GetWindow()->Activate();
    if (auto* glic_service =
            glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {
      glic_service->ToggleUI(browser_, /*prevent_close=*/true,
                             glic::mojom::InvocationSource::kActorTaskIcon);
      if (auto* instance = glic_service->GetInstanceForTab(last_tab)) {
        instance->NotifyActorTaskListRowClicked(task_id.value());
      }
    }
  }
  // Regardless of tab navigation, process the row and close the bubble when
  // done.
  auto* icon_manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile);
  icon_manager->ProcessRowInTaskListBubble(task_id);
  CloseBubble();
  actor::ui::LogTaskListBubbleRowClicked();
}

ActorTaskListBubbleControllerDelegate*
ActorTaskListBubbleController::GetActiveDelegate() const {
  return split_button_controller_->GetActiveDelegate();
}

// static
ActorTaskListBubbleController* ActorTaskListBubbleController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}
