// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/weak_document_ptr.h"

namespace actor {

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
  ToolController(TaskId task_id, AggregatedJournal& journal);
  ~ToolController();
  ToolController(const ToolController&) = delete;
  ToolController& operator=(const ToolController&) = delete;

  // Invokes a tool action.
  void Invoke(
      const ToolRequest& request,
      const optimization_guide::proto::AnnotatedPageContent* last_observation,
      ResultCallback result_callback);

 private:
  // Called when the tool itself finishes its invocation.
  void DidFinishToolInvoke(mojom::ActionResultPtr result);

  // Call to clear the current tool invocation and return the given result to
  // the initiator. Must only be called when a tool invocation is in-progress.
  void CompleteToolRequest(mojom::ActionResultPtr result);

  void ValidationComplete(mojom::ActionResultPtr result);

  // This state is non-null whenever a tool invocation is in progress.
  struct ActiveState {
    ActiveState(
        std::unique_ptr<Tool> tool,
        ResultCallback completion_callback,
        std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
        const optimization_guide::proto::AnnotatedPageContent*
            last_observation);
    ~ActiveState();
    ActiveState(const ActiveState&) = delete;
    ActiveState& operator=(const ActiveState&) = delete;

    // Both `tool` and `completion_callback` are guaranteed to be non-null while
    // active_state_ is set.
    std::unique_ptr<Tool> tool;
    ResultCallback completion_callback;
    std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry;
    raw_ptr<const optimization_guide::proto::AnnotatedPageContent>
        last_observation;
  };
  std::optional<ActiveState> active_state_;

  // Set while a tool invocation is in progress, delays invocation of the
  // completion_callback until the page is ready for observation.
  std::unique_ptr<ObservationDelayController> observation_delayer_;

  TaskId task_id_;

  base::SafeRef<AggregatedJournal> journal_;

  base::WeakPtrFactory<ToolController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_
