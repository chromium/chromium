// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/named_trigger.h"

#include "base/check.h"

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

}  // namespace base::trace_event
