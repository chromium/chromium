// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/named_trigger.h"

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/strings/strcat.h"
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

}  // namespace base::trace_event
