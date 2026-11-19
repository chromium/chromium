// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TASK_OPTIONS_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TASK_OPTIONS_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "chrome/common/glic_enums.mojom.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace actor {

class ActorTaskDelegate;

// Options for creating an ActorTask.
struct ActorTaskOptions {
  // Initial surface behavior when the task has no initial surface. kHeaded
  // creates no surface. kHeadless creates a headless surface for the task.
  enum class ExecutionMode {
    kHeaded,
    kHeadless,
  };

  ActorTaskOptions();
  ActorTaskOptions(const ActorTaskOptions&) = delete;
  ActorTaskOptions& operator=(const ActorTaskOptions&) = delete;
  ActorTaskOptions(ActorTaskOptions&&);
  ActorTaskOptions& operator=(ActorTaskOptions&&);
  ~ActorTaskOptions();

  // Optional task options from a WebUI client (e.g. title, duration, feature
  // mode, actuation tab ID).
  webui::mojom::TaskOptionsPtr webui_task_options;

  // Optional delegate for handling user interventions and task events.
  base::WeakPtr<ActorTaskDelegate> delegate;

  // Invocation source that first opened the Glic instance this task was
  // created from. std::nullopt for tasks not created via Glic.
  std::optional<glic::mojom::InvocationSource> glic_initial_invocation_source;

  // URL schemes the task is permitted to act on.
  ExecutionEngine::AllowedSchemes allowed_schemes =
      ExecutionEngine::AllowedSchemes::kRequireHttpsOrHttp;

  // Optional task-level origin gating policy configuration.
  std::optional<optimization_guide::proto::AgentContainerConfig>
      agent_container_config;

  // Initial surface creation mode when no initial surface is provided.
  ExecutionMode execution_mode = ExecutionMode::kHeaded;

  // Whether navigations and critical actions performed during the task should
  // be excluded from Chrome history and critical action logging.
  bool skip_history_recording = false;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TASK_OPTIONS_H_
