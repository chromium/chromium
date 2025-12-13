// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_prefs.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/ui_event_debugstring.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
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
      .actor_overlay = {.is_active = true, .border_glow_visible = true},
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator = TabIndicatorStatus::kDynamic,
      .border_glow_visible = true,
  };
  return kActorState;
}

const UiTabState& GetWaitingOnUserUiTabState() {
  static const UiTabState kActorState = {
      .actor_overlay = {.is_active = true, .border_glow_visible = true},
      .handoff_button = {.is_active = true, .controller = kActor},
      .tab_indicator = TabIndicatorStatus::kStatic,
      .border_glow_visible = true,
  };
  return kActorState;
}

const UiTabState& GetPausedUiTabState() {
  static const UiTabState kPausedState = {
      .actor_overlay = {.is_active = false, .border_glow_visible = false},
      .handoff_button = {.is_active = !base::FeatureList::IsEnabled(
                             features::kGlicHandoffButtonHiddenClientControl),
                         .controller = kClient},
      .tab_indicator = TabIndicatorStatus::kNone,
      .border_glow_visible = false,
  };
  return kPausedState;
}

const UiTabState& GetCompletedUiTabState() {
  static const UiTabState kCompletedState = {
      .actor_overlay = {.is_active = false, .border_glow_visible = false},
      .handoff_button = {.is_active = false, .controller = kClient},
      .tab_indicator = TabIndicatorStatus::kNone,
      .border_glow_visible = false,
  };
  return kCompletedState;
}

struct TabUiUpdate {
  raw_ptr<TabInterface> tab;
  UiTabState ui_tab_state;
};

auto GetNewUiStateFn() {
  return absl::Overload{
      [](const StartingToActOnTab& e) -> TabUiUpdate {
        return TabUiUpdate{e.tab_handle.Get(), GetActorControlledUiTabState()};
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
  std::move(complete_callback)
      .Run(result ? MakeOkResult()
                  : MakeResult(mojom::ActionResultCode::kActorUiError));
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

}  // namespace

ActorUiStateManager::ActorUiStateManager(ActorKeyedService& actor_service)
    : actor_service_(actor_service) {}
ActorUiStateManager::~ActorUiStateManager() = default;

// TODO(crbug.com/424495020): If the tab doesn't exist we will silently
// fail/not send a callback in the interim until these tasks are able to
// accept a callback.
void ActorUiStateManager::OnActorTaskStateChange(
    TaskId task_id,
    ActorTask::State new_task_state,
    const std::string& title) {
  TRACE_EVENT("actor", "UiStateManager::OnActorTaskStateChange", "new_state",
              new_task_state);
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
    case ActorTask::State::kWaitingOnUser:
      ui_tab_state = GetWaitingOnUserUiTabState();
      break;
    case ActorTask::State::kPausedByUser:
    case ActorTask::State::kPausedByActor:
      ui_tab_state = GetPausedUiTabState();
      break;
    case ActorTask::State::kFailed:
    case ActorTask::State::kCancelled:
    case ActorTask::State::kFinished:
      ui_tab_state = GetCompletedUiTabState();
      // TODO(crbug.com/458391262) revisit or cleanup implementation here for
      // m144.
      NotifyActorTaskStopped(task_id, new_task_state, title);
      break;
  }
  for (const auto& tab : GetTabs(task_id)) {
    if (auto* tab_controller = ActorUiTabControllerInterface::From(tab)) {
      tab_controller->OnUiTabStateChange(ui_tab_state,
                                         base::BindOnce(&LogUiChangeError));
    }
  }

  notify_actor_task_state_change_debounce_timer_.Start(
      FROM_HERE, kProfileScopedUiUpdateDebounceDelay,
      base::BindOnce(&ActorUiStateManager::NotifyActorTaskStateChange,
                     weak_factory_.GetWeakPtr(), task_id));
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
  TRACE_EVENT("actor", "UiStateManager::OnUiEvent_Async", "event",
              DebugString(event));
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    const TabUiUpdate update = std::visit(GetNewUiStateFn(), event);
    if (auto* tab_controller =
            ActorUiTabControllerInterface::From(update.tab)) {
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
  TRACE_EVENT("actor", "UiStateManager::OnUiEvent_Sync", "event",
              DebugString(event));
  if (!base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    return;
  }
  std::visit(
      absl::Overload{
          [this](const StartTask& e) {
            notify_actor_task_state_change_debounce_timer_.Start(
                FROM_HERE, kProfileScopedUiUpdateDebounceDelay,
                base::BindOnce(&ActorUiStateManager::NotifyActorTaskStateChange,
                               weak_factory_.GetWeakPtr(), e.task_id));
          },
          [this](const TaskStateChanged& e) {
            this->OnActorTaskStateChange(e.task_id, e.state, e.title);
          },
          [](const StoppedActingOnTab& e) {
            auto* tab = e.tab_handle.Get();
            if (auto* tab_controller =
                    ActorUiTabControllerInterface::From(tab)) {
              tab_controller->OnUiTabStateChange(
                  GetCompletedUiTabState(), base::BindOnce(&LogUiChangeError));
            }
          }},
      event);
}

void ActorUiStateManager::MaybeShowToast(BrowserWindowInterface* bwi) {
  if (!features::kGlicActorUiToast.Get()) {
    return;
  }

  if (!bwi || bwi->capabilities()->IsAttemptingToCloseBrowser()) {
    return;
  }

  PrefService* pref_service = actor_service_->GetProfile()->GetPrefs();
  int toast_shown_count = pref_service->GetInteger(kToastShown);

  DCHECK(toast_shown_count <= kToastShownMax)
      << "Toast shown count (" << toast_shown_count
      << ") is greater than the max allowed (" << kToastShownMax << ").";

  if (toast_shown_count >= kToastShownMax) {
    return;
  }

  auto ids = actor_service_->FindTaskIdsInActive([](const ActorTask& task) {
    return task.GetState() == ActorTask::State::kActing ||
           task.GetState() == ActorTask::State::kReflecting;
  });

  if (!ids.empty() && MaybeShowToastViaController(bwi)) {
    pref_service->SetInteger(kToastShown, toast_shown_count + 1);
  }
}

void ActorUiStateManager::NotifyActorTaskStateChange(TaskId task_id) {
  actor_task_state_change_callback_list_.Notify(task_id);
}

void ActorUiStateManager::NotifyActorTaskStopped(TaskId task_id,
                                                 ActorTask::State final_state,
                                                 const std::string& title) {
  actor_task_stopped_callback_list_.Notify(task_id, final_state, title);
}

base::CallbackListSubscription
ActorUiStateManager::RegisterActorTaskStateChange(
    ActorTaskStateChangeCallback callback) {
  return actor_task_state_change_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription ActorUiStateManager::RegisterActorTaskStopped(
    ActorTaskStoppedCallback callback) {
  return actor_task_stopped_callback_list_.Add(std::move(callback));
}

}  // namespace actor::ui
