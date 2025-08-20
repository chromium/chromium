// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_prefs.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#endif
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace actor::ui {
namespace {
// The maximum number of times the closing toast should be shown for a profile.
constexpr int kToastShownMax = 2;

using tabs::TabInterface;
using enum HandoffButtonState::ControlOwnership;

// TODO(crbug.com/424495020): Hardcoded states; Move this out to it's own file
// to be shared with tab controller.
const UiTabState& GetActorControlledUiTabState() {
  static const UiTabState kActorState = {
      .actor_overlay = ActorOverlayState(/*is_active=*/true),
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator_visible = true,
      .border_glow_visible = true,
  };
  return kActorState;
}

const UiTabState& GetPausedUiTabState() {
  static const UiTabState kPausedState = {
      .actor_overlay = ActorOverlayState(/*is_active=*/false),
      .handoff_button = {.is_active = true, .controller = kClient},
      .tab_indicator_visible = false,
      .border_glow_visible = false,
  };
  return kPausedState;
}

const UiTabState& GetCompletedUiTabState() {
  static const UiTabState kCompletedState = {
      .actor_overlay = ActorOverlayState(/*is_active=*/false),
      .handoff_button = {.is_active = false, .controller = kClient},
      .tab_indicator_visible = false,
      .border_glow_visible = false,
  };
  return kCompletedState;
}

struct TabUiUpdate {
  raw_ptr<TabInterface> tab;
  UiTabState ui_tab_state;
};

auto GetNewUiStateFn(ActorUiStateManager& manager) {
  return absl::Overload{
      [&manager](const StartingToActOnTab& e) -> TabUiUpdate {
        auto* tab = e.tab_handle.Get();
        if (auto* tab_controller = manager.GetUiTabController(tab)) {
          tab_controller->SetActiveTaskId(e.task_id);
        }
        return TabUiUpdate{tab, GetActorControlledUiTabState()};
      },
      [](const MouseClick& e) -> TabUiUpdate {
        UiTabState ui_tab_state = GetActorControlledUiTabState();
        ui_tab_state.actor_overlay.mouse_down = true;
        return TabUiUpdate{e.tab_handle.Get(), ui_tab_state};
      },
      [](const MouseMove& e) -> TabUiUpdate {
        UiTabState ui_tab_state = GetActorControlledUiTabState();
        ui_tab_state.actor_overlay.mouse_target = e.target;
        return TabUiUpdate{e.tab_handle.Get(), ui_tab_state};
      }};
}

// TODO(crbug.com/424495020): Bool may be converted to a map of ui
// components:bool depending on what controller returns.
void OnUiChangeComplete(UiCompleteCallback complete_callback, bool result) {
  std::move(complete_callback).Run(result ? MakeOkResult() : MakeErrorResult());
}

void LogUiChangeError(bool result) {
  if (!result) {
    LOG(DFATAL)
        << "Unexpected error when trying to update actor ui components.";
  }
}

bool MaybeShowToastViaController(BrowserWindowInterface* bwi) {
  if (auto* controller = bwi->GetFeatures().toast_controller()) {
    return controller->MaybeShowToast(
        ToastParams(ToastId::kGeminiWorkingOnTask));
  }
  return false;
}

bool IsRecentlyCompletedTask(const ActorTask& task) {
  bool is_finished = (task.GetState() == actor::ActorTask::State::kFinished);
  bool is_not_expired =
      (base::Time::Now() - task.GetEndTime() <
       base::Seconds(
           features::kGlicActorUiCompletedTaskExpiryDelaySeconds.Get()));
  return is_finished && is_not_expired;
}

}  // namespace

ActorUiStateManager::ActorUiStateManager(ActorKeyedService& actor_service)
    : actor_service_(actor_service) {}
ActorUiStateManager::~ActorUiStateManager() = default;

// TODO(crbug.com/424495020): If the tab doesn't exist we will silently
// fail/not send a callback in the interim until these tasks are able to
// accept a callback.
void ActorUiStateManager::OnActorTaskStateChange(
    TaskId task_id,
    ActorTask::State new_task_state) {
  // TODO(crbug.com/424495020): Look into converting this switch into a
  // map/catalog.
  // Notify tab-scoped UI components.
  UiTabState ui_tab_state;
  switch (new_task_state) {
    case ActorTask::State::kCreated:
      LOG(FATAL)
          << "Task state should never be set to kCreated from another state.";
    case ActorTask::State::kActing:
    case ActorTask::State::kReflecting:
      ui_tab_state = GetActorControlledUiTabState();
      break;
    case ActorTask::State::kPausedByUser:
    case ActorTask::State::kPausedByActor:
      ui_tab_state = GetPausedUiTabState();
      break;
    case ActorTask::State::kCancelled:
      ui_tab_state = GetCompletedUiTabState();
      break;
    case ActorTask::State::kFinished:
      ui_tab_state = GetCompletedUiTabState();
      completed_tasks_expiry_timer_.Start(
          FROM_HERE,
          base::Seconds(
              features::kGlicActorUiCompletedTaskExpiryDelaySeconds.Get()),
          base::BindOnce(
              &ActorUiStateManager::MaybeNotifyProfileScopedUiComponents,
              weak_factory_.GetWeakPtr()));
      break;
  }
  for (const auto& tab : GetTabs(task_id)) {
    if (auto* tab_controller = GetUiTabController(tab)) {
      tab_controller->OnUiTabStateChange(ui_tab_state,
                                         base::BindOnce(&LogUiChangeError));
    }
  }

  // Update profile scoped state change.
  update_profile_scoped_ui_debounce_timer_.Start(
      FROM_HERE, kProfileScopedUiUpdateDebounceDelay,
      base::BindOnce(&ActorUiStateManager::MaybeNotifyProfileScopedUiComponents,
                     weak_factory_.GetWeakPtr()));
}

ActorUiTabControllerInterface* ActorUiStateManager::GetUiTabController(
    tabs::TabInterface* tab) {
  if (!tab) {
    LOG(ERROR) << "Tab does not exist.";
    return nullptr;
  }
  auto* tab_controller = tab->GetTabFeatures()->actor_ui_tab_controller();
  DCHECK(tab_controller)
      << "TabController should always exist for a valid tab.";
  return tab_controller;
}

std::vector<tabs::TabInterface*> ActorUiStateManager::GetTabs(TaskId id) {
  if (ActorTask* task = actor_service_->GetTask(id)) {
    std::vector<tabs::TabInterface*> tabs;
    for (const tabs::TabHandle& handle : task->GetTabs()) {
      if (tabs::TabInterface* tab = handle.Get()) {
        tabs.push_back(tab);
      }
    }
    return tabs;
  }
  return {};
}

// TODO(crbug.com/424495020): In the future when a UiEvent can modify multiple
// scoped ui components, we can look into using BarrierClosure.
void ActorUiStateManager::OnUiEvent(AsyncUiEvent event,
                                    UiCompleteCallback callback) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    const TabUiUpdate update = std::visit(GetNewUiStateFn(*this), event);
    if (auto* tab_controller = GetUiTabController(update.tab)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ActorUiTabControllerInterface::OnUiTabStateChange,
              tab_controller->GetWeakPtr(), update.ui_tab_state,
              base::BindOnce(&OnUiChangeComplete, std::move(callback))));
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(callback),
              MakeResult(::actor::mojom::ActionResultCode::kTabWentAway)));
    }
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
  }
}

void ActorUiStateManager::OnUiEvent(SyncUiEvent event) {
  if (!base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    return;
  }
  std::visit(
      absl::Overload{[this](const StartTask& e) {
                       this->MaybeNotifyProfileScopedUiComponents();
                     },
                     [this](const TaskStateChanged& e) {
                       this->OnActorTaskStateChange(e.task_id, e.state);
                     },
                     [this](const StoppedActingOnTab& e) {
                       auto* tab = e.tab_handle.Get();
                       if (auto* tab_controller = GetUiTabController(tab)) {
                         tab_controller->ClearActiveTaskId();
                         tab_controller->OnUiTabStateChange(
                             GetCompletedUiTabState(),
                             base::BindOnce(&LogUiChangeError));
                       }
                     }},
      event);
}

#if BUILDFLAG(ENABLE_GLIC)
void ActorUiStateManager::OnGlicUpdateFloatyState(
    glic::GlicWindowController::State floaty_state,
    glic::mojom::CurrentView current_view) {
  UpdateTaskIconSuppressionOnFloatyStateChange(floaty_state, current_view);

  if (task_icon_state_ != TaskIconUiState::kHidden) {
    if (suppress_task_icon_text_) {
      task_icon_state_ = TaskIconUiState::kShown;
    }

    task_icon_change_callback_list_.Notify(task_icon_state_, floaty_state,
                                           current_view);
  }
}

void ActorUiStateManager::UpdateTaskIconSuppressionOnFloatyStateChange(
    glic::GlicWindowController::State floaty_state,
    glic::mojom::CurrentView current_view) {
  if (!suppress_task_icon_text_ &&
      ShouldSuppressTaskIconText(floaty_state, current_view)) {
    suppress_task_icon_text_ = true;
  }
}

bool ActorUiStateManager::ShouldSuppressTaskIconText(
    glic::GlicWindowController::State floaty_state,
    glic::mojom::CurrentView view) {
  return floaty_state == glic::GlicWindowController::State::kOpen &&
         view == glic::mojom::CurrentView::kActuation;
}

base::CallbackListSubscription ActorUiStateManager::RegisterTaskIconStateChange(
    TaskIconStateChangeCallback callback) {
  return task_icon_change_callback_list_.Add(std::move(callback));
}
#endif

void ActorUiStateManager::MaybeShowToast(BrowserWindowInterface* bwi) {
  if (!features::kGlicActorUiToast.Get()) {
    return;
  }

  PrefService* pref_service = actor_service_->GetProfile()->GetPrefs();
  int toast_shown_count = pref_service->GetInteger(kToastShown);
  if (toast_shown_count >= kToastShownMax) {
    return;
  }

  auto ids = actor_service_->FindTaskIdsInActive(
      base::BindRepeating([](const ActorTask& task) {
        return task.GetState() == ActorTask::State::kActing ||
               task.GetState() == ActorTask::State::kReflecting;
      }));

  if (!ids.empty() && MaybeShowToastViaController(bwi)) {
    pref_service->SetInteger(kToastShown, toast_shown_count + 1);
  }
}

void ActorUiStateManager::MaybeNotifyProfileScopedUiComponents() {
  auto paused_ids = actor_service_->FindTaskIdsInActive(
      base::BindRepeating([](const ActorTask& task) {
        return task.GetState() == ActorTask::State::kPausedByActor;
      }));

  auto completed_ids = actor_service_->FindTaskIdsInInactive(
      base::BindRepeating(&IsRecentlyCompletedTask));

  // TODO(crbug.com/437161973): Port this over to the dedicated TaskIcon keyed
  // service class.
  TaskIconUiState new_task_icon_state;
  if (!paused_ids.empty()) {
    new_task_icon_state = ActorUiStateManager::TaskIconUiState::kNeedsAttention;
  } else if (!completed_ids.empty()) {
    new_task_icon_state = ActorUiStateManager::TaskIconUiState::kCompleteTasks;
  } else if (!actor_service_->GetActiveTasks().empty()) {
    new_task_icon_state = ActorUiStateManager::TaskIconUiState::kShown;
  } else {
    new_task_icon_state = ActorUiStateManager::TaskIconUiState::kHidden;
  }

  if (task_icon_state_ != new_task_icon_state) {
    task_icon_state_ = new_task_icon_state;

// TODO(crbug.com/437161973): Refactor to remove this dependency post-m3 &
// post-task icon refactor, improve unit tests by injecting window_controller +
// host
#if BUILDFLAG(ENABLE_GLIC)
    if (auto* glic_keyed_service =
            glic::GlicKeyedServiceFactory::GetGlicKeyedService(
                actor_service_->GetProfile())) {
      if (!ShouldSuppressTaskIconText(
              glic_keyed_service->window_controller().state(),
              glic_keyed_service->host().GetPrimaryCurrentView())) {
        suppress_task_icon_text_ = false;
      } else if (task_icon_state_ !=
                 ActorUiStateManager::TaskIconUiState::kHidden) {
        // If the task icon text should be suppressed and isn't already hidden,
        // we should reset the task icon state.
        task_icon_state_ = ActorUiStateManager::TaskIconUiState::kShown;
      }
      task_icon_change_callback_list_.Notify(
          task_icon_state_, glic_keyed_service->window_controller().state(),
          glic_keyed_service->host().GetPrimaryCurrentView());
    }
#endif
  }
}

ActorUiStateManager::TaskIconUiState ActorUiStateManager::GetTaskIconUiState()
    const {
  return task_icon_state_;
}

}  // namespace actor::ui
