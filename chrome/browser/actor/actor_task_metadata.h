// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_METADATA_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_METADATA_H_

#include <vector>

#include "chrome/browser/actor/actor_container_config.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

namespace actor {

class ActorTaskMetadata {
 public:
  ActorTaskMetadata();
  ActorTaskMetadata(const ActorTaskMetadata&) = delete;
  ActorTaskMetadata(ActorTaskMetadata&&);
  explicit ActorTaskMetadata(const optimization_guide::proto::Actions& actions);
  ~ActorTaskMetadata();

  static ActorTaskMetadata WithAddedWritableMainframeOriginsForTesting(
      std::vector<url::Origin> origins);
  static ActorTaskMetadata WithAgentContainerConfigForTesting(
      optimization_guide::proto::AgentContainerConfig config_proto);

  const absl::flat_hash_set<url::Origin>& added_writable_mainframe_origins()
      const {
    return added_writable_mainframe_origins_;
  }

  ActorContainerConfig& actor_container_config() {
    return actor_container_config_;
  }

 private:
  absl::flat_hash_set<url::Origin> added_writable_mainframe_origins_;
  ActorContainerConfig actor_container_config_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_METADATA_H_
