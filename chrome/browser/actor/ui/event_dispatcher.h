// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_

#include "base/functional/callback.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class ToolRequest;

namespace ui {
// LINT.IfChange(ModelPageTargetType)
// These enum values are persisted to logs.  Do not renumber or reuse numeric
// values.
enum class ModelPageTargetType {
  kDomNode = 0,
  kPoint = 1,
  kMaxValue = kPoint,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ModelPageTargetType)

// LINT.IfChange(ComputedTargetResult)
// These enum values are persisted to logs.  Do not renumber or reuse numeric
// values.
enum class ComputedTargetResult {
  kSuccess = 0,
  kMissingActorTabData = 1,
  kMissingAnnotatedPageContent = 2,
  kTargetNotResolvedInApc = 3,
  kMaxValue = kTargetNotResolvedInApc,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ComputedTargetResult)

class ActorUiStateManagerInterface;

// This object is not thread safe; it expects to be called from a single thread.
class UiEventDispatcher {
 public:
  using UiCompleteCallback =
      base::OnceCallback<void(::actor::mojom::ActionResultPtr)>;
  struct AddTab {
    TaskId task_id;
    tabs::TabInterface::Handle handle;
  };
  using ActorTaskAsyncChange = std::variant<AddTab>;

  struct ChangeTaskState {
    TaskId task_id;
    ActorTask::State old_state;
    ActorTask::State new_state;
    std::string title;
  };
  struct RemoveTab {
    TaskId task_id;
    tabs::TabInterface::Handle handle;
  };
  // TODO(crbug.com/425784083): Add tab changes from ActorTask.
  using ActorTaskSyncChange = std::variant<ChangeTaskState, RemoveTab>;

  virtual ~UiEventDispatcher() = default;

  // Should be called before the ToolRequest is actuated.  Callback will be made
  // once the UI has completed its pre-tool.
  virtual void OnPreTool(const ToolRequest& tool_request,
                         UiCompleteCallback callback) = 0;

  // Should be called after the ToolRequest is actuated.  Callback will be made
  // once the UI has completed its post-tool.
  virtual void OnPostTool(const ToolRequest& tool_request,
                          UiCompleteCallback callback) = 0;

  // Should be called when a Tool changes the ActorTask.
  virtual void OnActorTaskAsyncChange(const ActorTaskAsyncChange& change,
                                      UiCompleteCallback callback) = 0;

  // Should be called when properties of an ActorTask change.
  virtual void OnActorTaskSyncChange(const ActorTaskSyncChange& change) = 0;
};

std::unique_ptr<UiEventDispatcher> NewUiEventDispatcher(
    ActorUiStateManagerInterface* ui_state_manager);

}  // namespace ui

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_EVENT_DISPATCHER_H_
