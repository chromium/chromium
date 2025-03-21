// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {
class ActorCoordinator;
}

namespace glic {

// Controls the interaction with the actor to complete an action.
class GlicActorController {
 public:
  GlicActorController();
  GlicActorController(const GlicActorController&) = delete;
  GlicActorController& operator=(const GlicActorController&) = delete;
  ~GlicActorController();

  // Invokes the actor to complete an action.
  void Act(FocusedTabData focused_tab_data,
           const optimization_guide::proto::BrowserAction& action,
           const mojom::GetTabContextOptions& options,
           glic::mojom::WebClientHandler::ActInFocusedTabCallback callback);

 private:
  // Handles the result of the action, returning new page context if necessary.
  void OnActionFinished(
      FocusedTabData focused_tab_data,
      const mojom::GetTabContextOptions& options,
      glic::mojom::WebClientHandler::ActInFocusedTabCallback callback,
      bool action_succeeded) const;

  void GetContextFromFocusedTab(
      FocusedTabData focused_tab_data,
      const mojom::GetTabContextOptions& options,
      glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback)
      const;

  base::WeakPtr<GlicActorController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::unique_ptr<actor::ActorCoordinator> actor_coordinator_;
  base::WeakPtrFactory<GlicActorController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_
