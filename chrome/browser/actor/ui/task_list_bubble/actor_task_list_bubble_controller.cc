// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"

#include <string>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/common/chrome_features.h"

DEFINE_USER_DATA(ActorTaskListBubbleController);

ActorTaskListBubbleController::ActorTaskListBubbleController(
    BrowserWindowInterface* browser_window)
    : browser_(browser_window),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  if (auto* manager = tabs::GlicActorTaskIconManagerFactory::GetForProfile(
          browser_->GetProfile())) {
    bubble_state_change_callback_subscription_.push_back(
        manager->RegisterTaskListBubbleStateChange(
            base::BindRepeating(&ActorTaskListBubbleController::OnStateUpdate,
                                base::Unretained(this))));
  }
}

ActorTaskListBubbleController::~ActorTaskListBubbleController() = default;

void ActorTaskListBubbleController::ShowBubble(views::View* anchor_view) {
  if (!browser_->IsActive()) {
    // Only show the bubble in the active window.
    return;
  }

  const auto& task_id_to_state =
      tabs::GlicActorTaskIconManagerFactory::GetForProfile(
          browser_->GetProfile())
          ->actor_task_list_bubble_rows();
  // Do not show bubble if there are no rows to show.
  if (task_id_to_state.empty()) {
    return;
  }
  bubble_widget_ = ActorTaskListBubble::ShowBubble(
      browser_->GetProfile(), anchor_view, task_id_to_state,
      base::BindRepeating(&ActorTaskListBubbleController::OnTaskRowClicked,
                          weak_ptr_factory_.GetWeakPtr()));

  // All rows may be skipped, in which case the bubble will not be shown.
  if (!bubble_widget_) {
    return;
  }

  if (widget_observation_.IsObserving()) {
    widget_observation_.Reset();
  }
  widget_observation_.Observe(bubble_widget_);

  actor::ui::RecordTaskListBubbleRows(task_id_to_state.size());

  on_bubble_shown_callback_list.Notify();
}

void ActorTaskListBubbleController::OnStateUpdate() {
  if (auto* browser_view = BrowserElementsViews::From(browser_)) {
    TabStripActionContainer* tab_strip_action_container =
        browser_view->GetViewAs<TabStripActionContainer>(
            kTabStripActionContainerElementId);
    if (tab_strip_action_container &&
        tab_strip_action_container->GetIsShowingGlicActorTaskIconNudge()) {
      ShowBubble(tab_strip_action_container->glic_actor_task_icon());
    }
  }
}

void ActorTaskListBubbleController::OnWidgetDestroyed(views::Widget* widget) {
  bubble_widget_ = nullptr;
  widget_observation_.Reset();

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
    int tab_index = last_tab->GetBrowserWindowInterface()
                        ->GetTabStripModel()
                        ->GetIndexOfTab(last_tab);
    last_tab->GetBrowserWindowInterface()->GetTabStripModel()->ActivateTabAt(
        tab_index);
    // Activate the window that the tab is in as it may not be the current one.
    last_tab->GetBrowserWindowInterface()->GetWindow()->Activate();
    if (auto* glic_service =
            glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)) {
      glic_service->ToggleUI(browser_, /*prevent_close=*/true,
                             glic::mojom::InvocationSource::kActorTaskIcon);
      if (auto* instance = glic_service->GetInstanceForTab(last_tab)) {
        instance->host().NotifyActorTaskListRowClicked(task_id.value());
      }
    }
  }
  // Regardless of tab navigation, process the row and close the bubble when
  // done.
  auto* icon_manager =
      tabs::GlicActorTaskIconManagerFactory::GetForProfile(profile);
  icon_manager->ProcessRowInTaskListBubble(task_id);
  if (bubble_widget_) {
    bubble_widget_->Close();
  }
  actor::ui::LogTaskListBubbleRowClicked();
}

// static
ActorTaskListBubbleController* ActorTaskListBubbleController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}
