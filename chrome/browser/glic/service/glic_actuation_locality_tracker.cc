// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_actuation_locality_tracker.h"

#include <cstdint>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/actor/core/task_source_info.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

namespace {

using ActiveTaskMap = std::map<actor::TaskId, const actor::ActorTask*>;

tabs::TabInterface* ResolveLiveTabForTask(
    const actor::ActorTask& task,
    const base::flat_set<tabs::TabHandle>* excluded_tabs = nullptr) {
  const auto is_live_controlled_tab = [&](tabs::TabHandle handle) {
    if (handle == tabs::TabHandle::Null() || !task.HasTab(handle)) {
      return false;
    }
    if (excluded_tabs && excluded_tabs->contains(handle)) {
      return false;
    }
    return handle.Get() != nullptr;
  };

  const tabs::TabHandle last_handle = task.GetLastActuatedTabHandle();
  if (is_live_controlled_tab(last_handle)) {
    return last_handle.Get();
  }

  tabs::TabHandle fallback_handle = tabs::TabHandle::Null();
  for (const tabs::TabHandle handle : task.GetTabs()) {
    if (is_live_controlled_tab(handle) &&
        (fallback_handle == tabs::TabHandle::Null() ||
         handle.raw_value() > fallback_handle.raw_value())) {
      fallback_handle = handle;
    }
  }
  return fallback_handle != tabs::TabHandle::Null() ? fallback_handle.Get()
                                                    : nullptr;
}

tabs::TabInterface* FindActiveGeminiActuationTabInTasks(
    const ActiveTaskMap& active_tasks,
    std::string_view conversation_id,
    const base::flat_set<tabs::TabHandle>* excluded_tabs = nullptr) {
  if (conversation_id.empty()) {
    return nullptr;
  }

  // Multiple sequential turns in the same conversation share a root
  // `conversation_id`. Iterate in reverse `TaskId` order so the most recently
  // started non-completed Gemini `ActorTask` with a live tab wins if an earlier
  // turn's teardown is still in flight.
  for (const auto& [task_id, task] : std::views::reverse(active_tasks)) {
    const std::optional<std::string> task_conversation_id =
        GetGeminiConversationId(task->source_info());
    if (task_conversation_id != conversation_id) {
      continue;
    }
    if (tabs::TabInterface* tab = ResolveLiveTabForTask(*task, excluded_tabs)) {
      return tab;
    }
  }
  return nullptr;
}

ActiveTaskMap GetActiveTasksForProfile(Profile* profile) {
  if (!profile || profile->ShutdownStarted()) {
    return {};
  }
  if (auto* actor_service = actor::ActorKeyedService::Get(profile)) {
    return actor_service->GetActiveTasks();
  }
  return {};
}

}  // namespace

std::optional<std::string> GetGeminiConversationId(
    const actor::TaskSourceInfo& source_info) {
  // TODO(crbug.com/585137698): Also match `kBrowserActuator` tasks whose
  // `TaskSourceInfo` carries a Gemini conversation source once
  // `ConversationContext` lands on `TaskSourceInfo`.
  if (source_info.type == actor::TaskSourceInfo::Client::kGlic &&
      source_info.id.has_value() && !source_info.id->empty()) {
    return *source_info.id;
  }
  return std::nullopt;
}

tabs::TabInterface* FindActiveGeminiActuationTab(
    Profile* profile,
    std::string_view conversation_id) {
  if (!profile || conversation_id.empty()) {
    return nullptr;
  }
  return FindActiveGeminiActuationTabInTasks(GetActiveTasksForProfile(profile),
                                             conversation_id);
}

GlicActuationLocalityTracker::GlicActuationLocalityTracker(
    Profile* profile,
    GlicInstanceImpl* instance)
    : profile_(profile), instance_(instance) {
  CHECK(instance_);
  conversation_id_ = instance_->conversation_id().value_or(std::string());
  instance_observation_.Observe(instance_);
  callbacks_.set_removal_callback(
      base::BindRepeating(&GlicActuationLocalityTracker::OnCallbackRemoved,
                          base::Unretained(this)));
}

GlicActuationLocalityTracker::~GlicActuationLocalityTracker() = default;

mojom::ActuationLocalityStatePtr GlicActuationLocalityTracker::GetCurrentState()
    const {
  if (last_emitted_state_) {
    return last_emitted_state_->Clone();
  }
  return ComputeLocalityState(ResolveActiveTab());
}

base::CallbackListSubscription
GlicActuationLocalityTracker::AddLocalityChangedCallback(
    LocalityChangedCallback callback) {
  const bool was_empty = callbacks_.empty();
  auto subscription = callbacks_.Add(std::move(callback));
  if (was_empty) {
    if (!conversation_id_.empty()) {
      EnsureActorSubscriptions();
    }
    tabs::TabInterface* actuation_tab = ResolveActiveTab();
    UpdateTabSubscriptions(actuation_tab);
    last_emitted_state_ = ComputeLocalityState(actuation_tab);
  }
  return subscription;
}

void GlicActuationLocalityTracker::OnInstanceVisibilityChanged(
    const GlicInstance& instance) {
  Refresh();
}

void GlicActuationLocalityTracker::OnInstanceConversationRegistered(
    const GlicInstance& instance) {
  conversation_id_ = instance_->conversation_id().value_or(std::string());
  if (callbacks_.empty()) {
    return;
  }
  if (!conversation_id_.empty()) {
    EnsureActorSubscriptions();
  }
  Refresh();
}

void GlicActuationLocalityTracker::OnInstanceTabAddedToTask(
    const GlicInstance& instance,
    actor::TaskId task_id) {
  Refresh();
}

tabs::TabInterface* GlicActuationLocalityTracker::ResolveActiveTab() const {
  if (conversation_id_.empty()) {
    return nullptr;
  }
  return FindActiveGeminiActuationTabInTasks(GetActiveTasksForProfile(profile_),
                                             conversation_id_,
                                             &detaching_tab_handles_);
}

mojom::ActuationLocalityStatePtr
GlicActuationLocalityTracker::ComputeLocalityState(
    tabs::TabInterface* actuation_tab) const {
  if (!actuation_tab) {
    return mojom::ActuationLocalityState::NewNotLocal(std::monostate{});
  }

  const int32_t tab_id = actuation_tab->GetHandle().raw_value();
  // Adjacency holds iff the live WebUI embedder is currently active in the side
  // panel on `actuation_tab` (true whether pinned or unpinned, and unaffected
  // by OS window focus). Checking `IsActiveEmbedder()` directly avoids false
  // adjacency when only an inactive placeholder (`GlicInactiveSidePanelUi`,
  // e.g. during `Hibernate()`, deactivation, or `kPeek`) is showing on
  // `actuation_tab` or while the live panel is in another tab/window or Floaty.
  const bool is_adjacent =
      instance_->IsActiveEmbedder(SidePanelEmbedderKey(*actuation_tab));

  return mojom::ActuationLocalityState::NewLocalTab(
      mojom::LocalActuationTab::New(is_adjacent, tab_id));
}

void GlicActuationLocalityTracker::EnsureActorSubscriptions() {
  if (!profile_ || profile_->ShutdownStarted() ||
      task_state_changed_subscription_) {
    return;
  }
  // TODO(crbug.com/587942594): Subscribe to an `ActorKeyedService` callback for
  // controlled-surface and source-ID updates so cross-instance background tab
  // additions/removals and late `SetSourceId()` backfills refresh without
  // waiting for a task state or visibility toggle.
  if (auto* actor_service = actor::ActorKeyedService::Get(profile_)) {
    task_state_changed_subscription_ =
        actor_service->AddTaskStateChangedCallback(base::BindRepeating(
            &GlicActuationLocalityTracker::OnActorTaskChanged,
            base::Unretained(this)));
    task_visibility_changed_subscription_ =
        actor_service->AddTaskVisibilityChangedCallback(base::BindRepeating(
            &GlicActuationLocalityTracker::OnActorTaskChanged,
            base::Unretained(this)));
  }
}

void GlicActuationLocalityTracker::StopTracking() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  task_state_changed_subscription_ = {};
  task_visibility_changed_subscription_ = {};
  tab_will_detach_subscription_ = {};
  tab_did_insert_subscription_ = {};
  observed_tab_handle_ = tabs::TabHandle::Null();
  detaching_tab_handles_.clear();
  last_emitted_state_.reset();
}

void GlicActuationLocalityTracker::OnCallbackRemoved() {
  if (!callbacks_.empty()) {
    return;
  }
  StopTracking();
}

void GlicActuationLocalityTracker::OnActorTaskChanged(actor::ActorTask& task) {
  if (callbacks_.empty() || conversation_id_.empty()) {
    return;
  }
  const std::optional<std::string> task_conv =
      GetGeminiConversationId(task.source_info());
  if (task_conv != conversation_id_) {
    return;
  }
  Refresh();
}

void GlicActuationLocalityTracker::OnObservedTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (!tab || reason != tabs::TabInterface::DetachReason::kDelete) {
    return;
  }
  const tabs::TabHandle handle = tab->GetHandle();
  detaching_tab_handles_.insert(handle);
  Refresh();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GlicActuationLocalityTracker::ClearDetachingTabAndRefresh,
                     weak_ptr_factory_.GetWeakPtr(), handle));
}

void GlicActuationLocalityTracker::OnObservedTabDidInsert(
    tabs::TabInterface* tab) {
  Refresh();
}

void GlicActuationLocalityTracker::ClearDetachingTabAndRefresh(
    tabs::TabHandle tab_handle) {
  detaching_tab_handles_.erase(tab_handle);
  Refresh();
}

void GlicActuationLocalityTracker::Refresh() {
  if (callbacks_.empty()) {
    return;
  }
  tabs::TabInterface* actuation_tab = ResolveActiveTab();
  UpdateTabSubscriptions(actuation_tab);
  mojom::ActuationLocalityStatePtr next_state =
      ComputeLocalityState(actuation_tab);
  if (last_emitted_state_ && last_emitted_state_->Equals(*next_state)) {
    return;
  }
  last_emitted_state_ = next_state->Clone();
  callbacks_.Notify(*next_state);
}

void GlicActuationLocalityTracker::UpdateTabSubscriptions(
    tabs::TabInterface* tab) {
  const tabs::TabHandle next_handle =
      tab ? tab->GetHandle() : tabs::TabHandle::Null();
  if (observed_tab_handle_ == next_handle) {
    return;
  }
  observed_tab_handle_ = next_handle;
  tab_will_detach_subscription_ = {};
  tab_did_insert_subscription_ = {};
  if (!tab) {
    return;
  }
  tab_will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
      &GlicActuationLocalityTracker::OnObservedTabWillDetach,
      base::Unretained(this)));
  tab_did_insert_subscription_ = tab->RegisterDidInsert(
      base::BindRepeating(&GlicActuationLocalityTracker::OnObservedTabDidInsert,
                          base::Unretained(this)));
}

}  // namespace glic
