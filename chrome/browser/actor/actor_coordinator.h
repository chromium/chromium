// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
#define CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/actor/tools/tool_controller.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace url {
class Origin;
}  // namespace url

namespace optimization_guide::proto {
class BrowserAction;
}  // namespace optimization_guide::proto

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
  void Act(tabs::TabInterface& tab,
           const optimization_guide::proto::BrowserAction& action,
           ActionResultCallback callback);

 private:
  void OnMayActOnTabResponse(
      base::WeakPtr<tabs::TabInterface> tab,
      const optimization_guide::proto::BrowserAction& action,
      const url::Origin& evaluated_origin,
      ActionResultCallback callback,
      bool may_act);

  ToolController tool_controller_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ActorCoordinator> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
