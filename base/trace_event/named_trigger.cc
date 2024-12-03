// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/named_trigger.h"

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/strings/strcat.h"

namespace base::trace_event {

// |g_named_trigger_manager| is intentionally leaked on shutdown.
NamedTriggerManager* g_named_trigger_manager = nullptr;

bool EmitNamedTrigger(const std::string& trigger_name,
                      std::optional<int32_t> value) {
  if (g_named_trigger_manager) {
    return g_named_trigger_manager->DoEmitNamedTrigger(trigger_name, value);
  }
  return false;
}

void NamedTriggerManager::SetInstance(NamedTriggerManager* manager) {
  DCHECK(g_named_trigger_manager == nullptr || manager == nullptr);
  g_named_trigger_manager = manager;
}

uint64_t TriggerFlowId(const std::string_view& name,
                       std::optional<int32_t> value) {
  size_t name_hash = base::FastHash(name);
  return base::HashInts(name_hash, static_cast<uint32_t>(value.value_or(0)));
}

perfetto::Flow TriggerFlow(const std::string_view& name,
                           std::optional<int32_t> value) {
  return perfetto::Flow::Global(TriggerFlowId(name, value));
}

}  // namespace base::trace_event
