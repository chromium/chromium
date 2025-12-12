// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "content/public/browser/weak_document_ptr.h"

namespace actor {

class ActorTask;
class AggregatedJournal;
class Tool;
class ToolRequest;

// Entry point into actor tool usage. ToolController is a profile-scoped,
// ExecutionEngine-owned object. This class routes a tool use request to the
// appropriate browser tool or to a corresponding executor in the renderer for
// page-level tools.
class ToolController {
 public:
  using ResultCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;

  enum class State {
    kInit = 0,
    kReady,  // Ready for CreateToolAndValidate().
    kCreating,
    kValidating,
    kPostValidate,
    kInvokable,  // Ready for Invoke().
    kPreInvoke,
    kInvoking,
    kPostInvoke,
  };

  ToolController(ActorTask& actor_task, ToolDelegate& tool_delegate);
  ~ToolController();
  ToolController(const ToolController&) = delete;
  ToolController& operator=(const ToolController&) = delete;

  // Invokes a tool action.
  void CreateToolAndValidate(
      const ToolRequest& request,
      ResultCallback callback);
  void Invoke(ResultCallback result_callback);
  void Cancel();

  static std::string StateToString(State state);

 private:
  void SetState(State state);

  // Called when the tool itself finishes its invocation.
  void DidFinishToolInvoke(mojom::ActionResultPtr result);

  // Call to clear the current tool invocation and return the given result to
  // the initiator. Must only be called when a tool invocation is in-progress.
  void CompleteToolRequest(mojom::ActionResultPtr result);

  void PostValidate(mojom::ActionResultPtr result);
  void PostUpdateTask(mojom::ActionResultPtr result);
  void PostInvokeTool(mojom::ActionResultPtr result);
  void WaitForObservation(mojom::ActionResultPtr action_result);
  void ObservationDelayComplete(
      mojom::ActionResultPtr action_result,
      ObservationDelayController::Result observation_result);

  AggregatedJournal& journal() { return tool_delegate_->GetJournal(); }

  State state_ = State::kInit;

  // This state is non-null whenever a tool invocation is in progress.
  struct ActiveState {
    ActiveState(
        std::unique_ptr<Tool> tool,
        ResultCallback completion_callback,
        std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry);
    ~ActiveState();
    ActiveState(const ActiveState&) = delete;
    ActiveState& operator=(const ActiveState&) = delete;

    // Both `tool` and `completion_callback` are guaranteed to be non-null while
    // active_state_ is set.
    // `completion_callback` holds two different callbacks over its lifetime:
    // a callback for when CreateToolAndValidate is finished and, next, a
    // callback for when Invoke is finished.
    std::unique_ptr<Tool> tool;
    ResultCallback completion_callback;
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry;
  };
  std::optional<ActiveState> active_state_;

  ObservationDelayController::PageStabilityConfig
      observation_page_stability_config_;

  // Set while a tool invocation is in progress, delays invocation of the
  // completion_callback until the page is ready for observation.
  std::unique_ptr<ObservationDelayController> observation_delayer_;

  // ActorTask indirectly owns `this`.
  raw_ptr<ActorTask> task_;

  raw_ref<ToolDelegate> tool_delegate_;

  base::WeakPtrFactory<ToolController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_
