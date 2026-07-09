// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task_metadata.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {

ActorTaskMetadata::ActorTaskMetadata() = default;

ActorTaskMetadata::ActorTaskMetadata(ActorTaskMetadata&&) = default;

ActorTaskMetadata::ActorTaskMetadata(
    const optimization_guide::proto::Actions& actions) {
  if (!actions.has_task_metadata()) {
    return;
  }
  const optimization_guide::proto::TaskMetadata& task_metadata =
      actions.task_metadata();
  if (!task_metadata.has_security()) {
    return;
  }
  const optimization_guide::proto::SecurityMetadata& security =
      task_metadata.security();
  for (const std::string_view added_origin :
       security.added_writable_mainframe_origins()) {
    auto parsed_origin = url::Origin::Create(GURL(added_origin));
    if (!parsed_origin.opaque()) {
      added_writable_mainframe_origins_.insert(std::move(parsed_origin));
    }
  }
  if (task_metadata.security().has_agent_container_config()) {
    agent_container_config_.emplace(
        task_metadata.security().agent_container_config());
  }
}

ActorTaskMetadata::~ActorTaskMetadata() = default;

ActorTaskMetadata
ActorTaskMetadata::WithAddedWritableMainframeOriginsForTesting(
    std::vector<url::Origin> origins) {
  ActorTaskMetadata metadata;
  for (auto origin : origins) {
    metadata.added_writable_mainframe_origins_.insert(std::move(origin));
  }
  return metadata;
}

ActorTaskMetadata ActorTaskMetadata::WithAgentContainerConfigForTesting(
    optimization_guide::proto::AgentContainerConfig config_proto) {
  ActorTaskMetadata metadata;
  metadata.agent_container_config_.emplace(std::move(config_proto));
  return metadata;
}

}  // namespace actor
