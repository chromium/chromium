// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"

#include <string>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "ui/base/l10n/l10n_util.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#endif

DEFINE_USER_DATA(ActorTaskListBubbleController);

ActorTaskListBubbleController::ActorTaskListBubbleController(
    BrowserWindowInterface* browser_window)
    : browser_(browser_window),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
#if BUILDFLAG(ENABLE_GLIC)
  if (auto* manager = tabs::GlicActorTaskIconManagerFactory::GetForProfile(
          browser_->GetProfile())) {
    bubble_state_change_callback_subscription_.push_back(
        manager->RegisterTaskListBubbleStateChange(
            base::BindRepeating(&ActorTaskListBubbleController::OnStateUpdate,
                                base::Unretained(this))));
  }
#endif
}

ActorTaskListBubbleController::~ActorTaskListBubbleController() = default;

#if BUILDFLAG(ENABLE_GLIC)
void ActorTaskListBubbleController::ShowBubble(views::View* anchor_view) {
  if (!browser_->IsActive()) {
    // Only show the bubble in the active window.
    return;
  }
  auto task_id_to_state = tabs::GlicActorTaskIconManagerFactory::GetForProfile(
                              browser_->GetProfile())
                              ->GetActorTaskListBubbleRows();
  std::vector<ActorTaskListBubbleRowButtonParams> param_list;
  for (const auto& task : task_id_to_state) {
    param_list.emplace_back(CreateRowButtonParamsForTaskState(task.second));
  }
  bubble_widget_ =
      ActorTaskListBubble::ShowBubble(anchor_view, std::move(param_list));
  if (widget_observation_.IsObserving()) {
    widget_observation_.Reset();
  }
  widget_observation_.Observe(bubble_widget_);
}

ActorTaskListBubbleRowButtonParams
ActorTaskListBubbleController::CreateRowButtonParamsForTaskState(
    tabs::ActorTaskListBubbleRowState task_state) {
  return ActorTaskListBubbleRowButtonParams{
      .title = base::UTF8ToUTF16(task_state.title),
      .subtitle = l10n_util::GetStringUTF16(
          IDR_ACTOR_TASK_LIST_BUBBLE_CHECK_TASK_SUBTITLE),
      .on_click_callback = base::BindRepeating(
          &ActorTaskListBubbleController::GetOnTaskRowClickCallback,
          base::Unretained(this), task_state.task_id),
  };
}

void ActorTaskListBubbleController::OnStateUpdate(actor::TaskId task_id) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ActorTaskListBubbleController::OnStateUpdateImpl,
                     weak_ptr_factory_.GetWeakPtr(), task_id));
}

void ActorTaskListBubbleController::OnStateUpdateImpl(actor::TaskId task_id) {
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
#endif

void ActorTaskListBubbleController::OnWidgetDestroyed(views::Widget* widget) {
  bubble_widget_ = nullptr;
  widget_observation_.Reset();
}

void ActorTaskListBubbleController::GetOnTaskRowClickCallback(
    actor::TaskId task_id) {
#if BUILDFLAG(ENABLE_GLIC)
  Profile* profile = browser_->GetProfile();
  auto* icon_manager =
      tabs::GlicActorTaskIconManagerFactory::GetForProfile(profile);
  if (tabs::TabInterface* last_tab =
          icon_manager->GetLastUpdatedTabForTaskId(task_id)) {
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
    }
  }
  // Regardless of tab navigation, remove the row and close the bubble when
  // done.
  icon_manager->RemoveRowFromTaskListBubble(task_id);
  if (bubble_widget_) {
    bubble_widget_->Close();
  }
#endif
}

// static
ActorTaskListBubbleController* ActorTaskListBubbleController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}
