// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"

namespace actor {
class ActorTask;
}  // namespace actor

namespace glic {

// Controls the interaction with the actor to complete an action.
class GlicActorController {
 public:
  explicit GlicActorController(Profile* profile);
  GlicActorController(const GlicActorController&) = delete;
  GlicActorController& operator=(const GlicActorController&) = delete;
  ~GlicActorController();

  void StopTask(actor::TaskId task_id);
  void PauseTask(actor::TaskId task_id);
  void ResumeTask(
      actor::TaskId task_id,
      const mojom::GetTabContextOptions& context_options,
      glic::mojom::WebClientHandler::ResumeActorTaskCallback callback);

 private:
  actor::ActorTask* GetCurrentTask() const;

  raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_
