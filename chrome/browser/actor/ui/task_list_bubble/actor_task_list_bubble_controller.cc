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
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"
#include "chrome/browser/glic/browser_ui/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_USER_DATA(ActorTaskListBubbleController);

ActorTaskListBubbleController::ActorTaskListBubbleController(
    BrowserWindowInterface* browser_window)
    : browser_(browser_window),
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
  bubble_ = std::make_unique<ActorTaskListBubble>(
      browser_->GetProfile(), *this, manager->actor_task_list_bubble_rows(),
      base::BindRepeating(&ActorTaskListBubbleController::OnTaskRowClicked,
                          base::Unretained(this)));
}

ActorTaskListBubbleController::~ActorTaskListBubbleController() = default;

void ActorTaskListBubbleController::ShowBubble(views::View* anchor_view,
                                               bool is_start_notification) {
  const bool is_icon_hidden = anchor_view && !anchor_view->GetVisible();
  const bool should_delay = is_icon_hidden && is_start_notification &&
                            base::FeatureList::IsEnabled(
                                features::kGlicActorUiTaskListBubbleDelayShow);

  if (should_delay) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ActorTaskListBubbleController::ShowBubbleImpl,
                       weak_ptr_factory_.GetWeakPtr(), anchor_view,
                       is_start_notification),
        base::Milliseconds(features::kGlicActorUiTaskListBubbleDelayMs.Get()));
  } else {
    ShowBubbleImpl(anchor_view, is_start_notification);
  }
}

void ActorTaskListBubbleController::CloseBubble() {
  bubble_->Close();
}

bool ActorTaskListBubbleController::IsBubbleShowing() const {
  return bubble_->IsShowing();
}

void ActorTaskListBubbleController::ShowBubbleImpl(views::View* anchor_view,
                                                   bool is_start_notification) {
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
  if (bubble_) {
    bubble_->Close();
  }
  bubble_->Show(anchor_view);

  // All rows may be skipped, in which case the bubble will not be shown.
  if (bubble_->IsShowing()) {
    on_bubble_shown_callback_list.Notify();
  }
}

void ActorTaskListBubbleController::OnStateUpdate(bool is_start_notification) {
  if (auto* browser_view = BrowserElementsViews::From(browser_)) {
    auto* vertical_tab_strip_state_controller =
        tabs::VerticalTabStripStateController::From(browser_);
    if (vertical_tab_strip_state_controller->ShouldDisplayVerticalTabs()) {
      ToolbarView* toolbar_view =
          browser_view->GetViewAs<ToolbarView>(ToolbarView::kToolbarElementId);
      if (toolbar_view && (toolbar_view->GetIsShowingGlicActorTaskIconNudge() ||
                           is_start_notification)) {
        ShowBubble(toolbar_view->glic_actor_task_icon(), is_start_notification);
      }
    } else {
      TabStripActionContainer* tab_strip_action_container =
          browser_view->GetViewAs<TabStripActionContainer>(
              kTabStripActionContainerElementId);
      if (tab_strip_action_container &&
          (tab_strip_action_container->GetIsShowingGlicActorTaskIconNudge() ||
           is_start_notification)) {
        ShowBubble(tab_strip_action_container->glic_actor_task_icon(),
                   is_start_notification);
      }
    }
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
        instance->NotifyActorTaskListRowClicked(task_id.value());
      }
    }
  }
  // Regardless of tab navigation, process the row and close the bubble when
  // done.
  auto* icon_manager =
      glic::GlicActorTaskIconManagerFactory::GetForProfile(profile);
  icon_manager->ProcessRowInTaskListBubble(task_id);
  bubble_->Close();
  actor::ui::LogTaskListBubbleRowClicked();
}

// static
ActorTaskListBubbleController* ActorTaskListBubbleController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}
