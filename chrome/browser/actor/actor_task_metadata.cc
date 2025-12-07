// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task_metadata.h"

#include "url/gurl.h"

namespace actor {

ActorTaskMetadata::ActorTaskMetadata() = default;

ActorTaskMetadata::ActorTaskMetadata(ActorTaskMetadata&&) = default;

ActorTaskMetadata& ActorTaskMetadata::operator=(const ActorTaskMetadata&) =
    default;

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

}  // namespace actor
