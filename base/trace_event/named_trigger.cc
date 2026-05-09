// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/named_trigger.h"

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"

namespace base::trace_event {
namespace {

// |g_named_trigger_manager| is intentionally leaked on shutdown.
NamedTriggerManager* g_named_trigger_manager = nullptr;

}  // namespace

bool EmitNamedTrigger(const std::string& trigger_name,
                      std::optional<int32_t> value,
                      std::optional<uint64_t> flow_id) {
  if (g_named_trigger_manager) {
    return g_named_trigger_manager->DoEmitNamedTrigger(
        trigger_name, value,
        flow_id.value_or(base::trace_event::GetNextGlobalTraceId()));
  }
  return false;
}

void NamedTriggerManager::SetInstance(NamedTriggerManager* manager) {
  DCHECK(g_named_trigger_manager == nullptr || manager == nullptr);
  g_named_trigger_manager = manager;
}

NamedTriggerManager::NamedTriggerManager() = default;
NamedTriggerManager::~NamedTriggerManager() = default;

// static
NamedTriggerManager* NamedTriggerManager::GetInstance() {
  return g_named_trigger_manager;
}

void NamedTriggerManager::AddObserver(const std::string& name,
                                      Observer* observer) {
  observers_[name].AddObserver(observer);
}

void NamedTriggerManager::RemoveObserver(const std::string& name,
                                         Observer* observer) {
  observers_[name].RemoveObserver(observer);
}

void NamedTriggerManager::ClearObserversForTesting() {
  observers_.clear();
}

bool NamedTriggerManager::NotifyObservers(const std::string& trigger_name,
                                          std::optional<int32_t> value,
                                          uint64_t flow_id) {
  auto it = observers_.find(trigger_name);
  if (it == observers_.end()) {
    return false;
  }
  for (Observer& obs : it->second) {
    if (obs.OnNamedTrigger(value, flow_id)) {
      TRACE_EVENT_INSTANT("tracing.background", "NamedTrigger",
                          perfetto::Flow::Global(flow_id));
      return true;
    }
  }
  return false;
}

}  // namespace base::trace_event
