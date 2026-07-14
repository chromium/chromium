// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_container_config_slot.h"

#include "base/types/optional_ref.h"
#include "chrome/browser/actor/actor_container_config.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace actor {

ActorContainerConfigSlot::ActorContainerConfigSlot() = default;
ActorContainerConfigSlot::~ActorContainerConfigSlot() = default;

void ActorContainerConfigSlot::Assign(
    base::optional_ref<const optimization_guide::proto::AgentContainerConfig>
        config) {
  if (config_.has_value()) {
    return;
  }
  if (config.has_value()) {
    config_.emplace(*config);
  }
}

}  // namespace actor
