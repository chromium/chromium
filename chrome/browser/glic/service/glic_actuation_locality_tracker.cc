// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_actuation_locality_tracker.h"

#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/profiles/profile.h"
#include "components/actor/core/task_source_info.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

namespace {

using ActiveTaskMap = std::map<actor::TaskId, const actor::ActorTask*>;

tabs::TabInterface* ResolveLiveTabForTask(const actor::ActorTask& task) {
  const auto is_live_controlled_tab = [&](tabs::TabHandle handle) {
    return handle != tabs::TabHandle::Null() && task.HasTab(handle) &&
           handle.Get() != nullptr;
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
    std::string_view conversation_id) {
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
    if (tabs::TabInterface* tab = ResolveLiveTabForTask(*task)) {
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

}  // namespace glic
