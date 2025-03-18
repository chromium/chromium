// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
#define CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_

#include <cstdint>

#include "base/functional/callback_forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace tabs {
class TabInterface;
}

namespace actor {

// Coordinates the execution of a multi-step task.
class ActorCoordinator {
 public:
  using ActionResultCallback = base::OnceCallback<void(bool)>;

  ActorCoordinator();
  ActorCoordinator(const ActorCoordinator&) = delete;
  ActorCoordinator& operator=(const ActorCoordinator&) = delete;
  ~ActorCoordinator();

  // Performs the next action.
  void Act(tabs::TabInterface* tab,
           const optimization_guide::proto::BrowserAction& action,
           ActionResultCallback callback);
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
