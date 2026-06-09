// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/ui_event_debugstring.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor_webui.mojom.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/actor_tab_strip_tracker_desktop.h"
#endif

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#include "chrome/browser/actor/ui/actor_ui_state_manager_prefs.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#endif

// TODO(crbug.com/519294360): Implement ActorAndroidUITracker. Since
// TabStripModelObserver is desktop-only, the mobile equivalent on Android will
// implement TabModelObserver and TabModelListObserver to monitor tab events in
// real-time.

namespace actor::ui {
namespace {
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
// The maximum number of times the closing toast should be shown for a profile.
constexpr int kToastShownMax = 2;
#endif

using tabs::TabInterface;
using enum HandoffButtonState::ControlOwnership;

// TODO(crbug.com/424495020): Hardcoded states; Move this out to it's own file
// to be shared with tab controller.
constexpr UiTabState kDefaultActorState = {
    .actor_overlay = {.is_active = true, .border_glow_visible = true},
    .handoff_button = {.is_active = true, .controller = kActor},
    .tab_indicator = TabIndicatorStatus::kDynamic,
    .border_glow_visible = true,
};

constexpr UiTabState kTransientActorState = {
    .actor_overlay = {.is_active = false, .border_glow_visible = false},
    .handoff_button = {.is_active = false, .controller = kActor},
    .tab_indicator = TabIndicatorStatus::kDynamic,
    .border_glow_visible = false,
};

constexpr UiTabState kWaitingOnUserUiTabState = {
    .actor_overlay = {.is_active = true, .border_glow_visible = true},
    .handoff_button = {.is_active = true, .controller = kActor},
    .tab_indicator = TabIndicatorStatus::kStatic,
    .border_glow_visible = true,
};

constexpr UiTabState kPausedUiTabState = {
    .actor_overlay = {.is_active = false, .border_glow_visible = false},
    .handoff_button = {.is_active = false, .controller = kClient},
    .tab_indicator = TabIndicatorStatus::kNone,
    .border_glow_visible = false,
};

constexpr UiTabState kCompletedUiTabState = {
    .actor_overlay = {.is_active = false, .border_glow_visible = false},
    .handoff_button = {.is_active = false, .controller = kClient},
    .tab_indicator = TabIndicatorStatus::kNone,
    .border_glow_visible = false,
};

struct TabUiUpdate {
  raw_ptr<TabInterface> tab;
  UiTabState ui_tab_state;
};

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

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
bool MaybeShowToastViaController(BrowserWindowInterface* bwi) {
  if (auto* controller = bwi->GetFeatures().toast_controller()) {
    return controller->MaybeShowToast(
        ToastParams(ToastId::kGeminiWorkingOnTask));
  }
  return false;
}
#endif

}  // namespace

ActorUiStateManager::ActorUiStateManager(ActorKeyedService& actor_service)
    : actor_service_(actor_service) {}
ActorUiStateManager::~ActorUiStateManager() = default;

ActorTask::TaskDuration ActorUiStateManager::GetDuration(TaskId task_id) {
  if (ActorTask* task = actor_service_->GetTask(task_id)) {
    return task->get_task_duration();
  }
  if (auto stopped_task = stopped_task_info_.find(task_id);
      stopped_task != stopped_task_info_.end()) {
    return stopped_task->second.duration;
  }
  return ActorTask::TaskDuration::kDefault;
}

glic::mojom::FeatureMode ActorUiStateManager::GetFeatureMode(TaskId task_id) {
  if (ActorTask* task = actor_service_->GetTask(task_id)) {
    return task->feature_mode();
  }
  if (auto stopped_task = stopped_task_info_.find(task_id);
      stopped_task != stopped_task_info_.end()) {
    return stopped_task->second.feature_mode;
  }
  return glic::mojom::FeatureMode::kUnspecified;
}

ActorTask::TaskDuration ActorUiStateManager::GetDuration(
    const tabs::TabInterface* tab) {
  if (tab) {
    if (ActorTask* task = actor_service_->GetTaskFromTab(*tab)) {
      return task->get_task_duration();
    }
  }
  return ActorTask::TaskDuration::kDefault;
}

UiTabState ActorUiStateManager::GetActorControlledUiTabState(TaskId task_id) {
  switch (GetDuration(task_id)) {
    case ActorTask::TaskDuration::kDefault:
      return kDefaultActorState;
    case ActorTask::TaskDuration::kTransient: {
      auto it = transient_task_timers_.find(task_id);
      if (it != transient_task_timers_.end() && !it->second->IsRunning()) {
        return kDefaultActorState;
      }
      return kTransientActorState;
    }
  }
  NOTREACHED();
}

UiTabState ActorUiStateManager::GetActorControlledUiTabState(
    const tabs::TabInterface* tab) {
  if (ActorTask* task = tab ? actor_service_->GetTaskFromTab(*tab) : nullptr) {
    return GetActorControlledUiTabState(task->id());
  }
  return kDefaultActorState;
}

void ActorUiStateManager::OnTransientTaskDelayExpired(TaskId task_id) {
  std::optional<ActorTask::State> state = GetActorTaskState(task_id);
  if (state && (*state == ActorTask::State::kActing ||
                *state == ActorTask::State::kReflecting)) {
    OnActorTaskStateChange(task_id, *state);
  }
}

void ActorUiStateManager::StopTimer(TaskId task_id) {
  auto it = transient_task_timers_.find(task_id);
  if (it != transient_task_timers_.end()) {
    it->second->Stop();
  } else {
    // If the timer doesn't exist yet (e.g. we transitioned to a waiting/paused
    // state before any acting started), we insert a stopped (not running)
    // timer.  This will trigger the default (non-transient) UI.
    auto timer = std::make_unique<base::OneShotTimer>();
    transient_task_timers_[task_id] = std::move(timer);
  }
}

// TODO(crbug.com/424495020): If the tab doesn't exist we will silently
// fail/not send a callback in the interim until these tasks are able to
// accept a callback.
void ActorUiStateManager::OnActorTaskStateChange(
    TaskId task_id,
    ActorTask::State new_task_state) {
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
      if (GetDuration(task_id) == ActorTask::TaskDuration::kTransient) {
        if (!transient_task_timers_.contains(task_id)) {
          auto timer = std::make_unique<base::OneShotTimer>();
          timer->Start(
              FROM_HERE, actor::kGlicActorTransientTasksDelay.Get(),
              base::BindOnce(&ActorUiStateManager::OnTransientTaskDelayExpired,
                             weak_factory_.GetWeakPtr(), task_id));
          transient_task_timers_[task_id] = std::move(timer);
        }
      }
      ui_tab_state = GetActorControlledUiTabState(task_id);
      break;
    case ActorTask::State::kWaitingOnUser:
      StopTimer(task_id);
      ui_tab_state = kWaitingOnUserUiTabState;
      break;
    case ActorTask::State::kPausedByUser:
    case ActorTask::State::kPausedByActor:
      StopTimer(task_id);
      ui_tab_state = kPausedUiTabState;
      break;
    case ActorTask::State::kFailed:
    case ActorTask::State::kCancelled:
    case ActorTask::State::kFinished:
      LOG(FATAL) << "Stopped states should be processed via StopTask event.";
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
    const TabUiUpdate update = std::visit(
        absl::Overload{[this](const StartingToActOnTab& e) -> TabUiUpdate {
                         return TabUiUpdate{
                             e.tab_handle.Get(),
                             GetActorControlledUiTabState(e.task_id)};
                       },
                       [this](const MouseClick& e) -> TabUiUpdate {
                         UiTabState ui_tab_state =
                             GetActorControlledUiTabState(e.tab_handle.Get());
                         ui_tab_state.actor_overlay.mouse_down = true;
                         return TabUiUpdate{e.tab_handle.Get(), ui_tab_state};
                       },
                       [this](const MouseMove& e) -> TabUiUpdate {
                         UiTabState ui_tab_state =
                             GetActorControlledUiTabState(e.tab_handle.Get());
                         ui_tab_state.actor_overlay.mouse_target = e.target;
                         return TabUiUpdate{e.tab_handle.Get(), ui_tab_state};
                       }},
        event);
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
            this->OnActorTaskStateChange(e.task_id, e.state);
          },
          [this](const StopTask& e) {
            transient_task_timers_.erase(e.task_id);
            // Cancelled tasks are intentionally not stored.
            if (e.final_state == ActorTask::State::kCancelled) {
              NotifyActorTaskStopped(e.task_id);
              return;
            }
            stopped_task_info_.emplace(
                e.task_id,
                StoppedTaskInfo{
                    .final_state = e.final_state,
                    .title = e.title,
                    .last_acted_on_tab_handle = e.last_acted_on_tab_handle,
                    .duration = e.duration,
                    .feature_mode = e.feature_mode,
                });
            NotifyActorTaskStopped(e.task_id);

            // After expiry, remove the task and notify observers.
            base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
                FROM_HERE,
                base::BindOnce(&ActorUiStateManager::ActorTaskRemoved,
                               weak_factory_.GetWeakPtr(), e.task_id),
                base::Seconds(
                    features::kGlicActorUiCompletedTaskExpiryDelaySeconds
                        .Get()));
          },
          [](const StoppedActingOnTab& e) {
            auto* tab = e.tab_handle.Get();
            if (auto* tab_controller =
                    ActorUiTabControllerInterface::From(tab)) {
              tab_controller->OnUiTabStateChange(
                  kCompletedUiTabState, base::BindOnce(&LogUiChangeError));
            }
          }},
      event);
}

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
void ActorUiStateManager::MaybeShowToast(BrowserWindowInterface* bwi) {
  if (!base::FeatureList::IsEnabled(features::kGlicActorUi) ||
      !features::kGlicActorUiToast.Get()) {
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
#endif

void ActorUiStateManager::NotifyActorTaskStateChange(TaskId task_id) {
  actor_task_state_change_callback_list_.Notify(task_id);
}

void ActorUiStateManager::NotifyActorTaskStopped(TaskId task_id) {
  actor_task_stopped_callback_list_.Notify(task_id);
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

void ActorUiStateManager::ActorTaskRemoved(TaskId task_id) {
  stopped_task_info_.erase(task_id);
  actor_task_removed_callback_list_.Notify(task_id);
}

base::CallbackListSubscription ActorUiStateManager::RegisterActorTaskRemoved(
    ActorTaskRemovedCallback callback) {
  return actor_task_removed_callback_list_.Add(std::move(callback));
}

std::optional<std::string> ActorUiStateManager::GetActorTaskTitle(TaskId id) {
  if (ActorTask* task = actor_service_->GetTask(id)) {
    return task->title();
  }
  if (auto it = stopped_task_info_.find(id); it != stopped_task_info_.end()) {
    return it->second.title;
  }
  return std::nullopt;
}

std::optional<raw_ptr<tabs::TabInterface>>
ActorUiStateManager::GetLastActedOnTab(TaskId id) {
  if (ActorTask* task = actor_service_->GetTask(id)) {
    actor::ActorTask::TabHandleSet tabs = task->GetLastActedTabs();
    // TODO(crbug.com/441064175): Will need to be updated for multi-tab
    // actuation.
    return tabs.empty() ? nullptr : tabs.begin()->Get();
  }
  if (auto it = stopped_task_info_.find(id); it != stopped_task_info_.end()) {
    return it->second.last_acted_on_tab_handle.Get();
  }
  return std::nullopt;
}

std::optional<actor::ActorTask::State> ActorUiStateManager::GetActorTaskState(
    TaskId id) {
  if (ActorTask* task = actor_service_->GetTask(id)) {
    return task->GetState();
  }
  if (auto it = stopped_task_info_.find(id); it != stopped_task_info_.end()) {
    return it->second.final_state;
  }
  return std::nullopt;
}

size_t ActorUiStateManager::GetInactiveTaskCount() {
  return stopped_task_info_.size();
}

#if !BUILDFLAG(IS_ANDROID)
void ActorUiStateManager::LazyInitTabTracker() {
  if (!tab_strip_tracker_) {
    tab_strip_tracker_ = std::make_unique<actor::ActorTabStripTrackerDesktop>(
        actor_service_.get());
  }
}
#endif

}  // namespace actor::ui
