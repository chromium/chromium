// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_ACTIONS_RUNNER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_ACTIONS_RUNNER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"

class Profile;

namespace actor {

class ActorTask;
class TabObservationStrategy;
struct ActionResultWithLatencyInfo;

// Executes a batch of actions from `optimization_guide::proto::Actions` within
// a transient `ActorTask`.
//
// This runner is intended for one-shot, transient execution of an action batch
// without a multi-turn conversation loop. It manages the full lifecycle of the
// transient actor task: creating the task, injecting the target tab ID into
// actions when provided, dispatching the actions to `ActorKeyedService`,
// gathering post-action observations, populating the
// `optimization_guide::proto::ActionsResult`, and tearing down the task upon
// completion or destruction.
//
// Example usage:
//
// ```cpp
// void MyClient::RunActions(optimization_guide::proto::Actions actions,
//                           int32_t tab_id) {
//   actor::TaskSourceInfo source_info(
//       actor::TaskSourceInfo::Client::kContextualTasks,
//       "my-session-id");
//
//   actions_runner_ = std::make_unique<actor::ActorActionsRunner>(
//       *profile_, std::move(source_info), std::move(actions),
//       base::BindOnce(&MyClient::OnActionsComplete,
//                      weak_ptr_factory_.GetWeakPtr()),
//       tab_id);
//   actions_runner_->Start();
// }
//
// void MyClient::OnActionsComplete() {
//   std::unique_ptr<optimization_guide::proto::ActionsResult> result =
//       actions_runner_->TakeResult();
//   if (result) {
//     // Handle result.
//   }
//   actions_runner_.reset();
// }
// ```
class ActorActionsRunner {
 public:
  // Constructs an ActorActionsRunner.
  // - `profile`: Profile used to look up `ActorKeyedService`.
  // - `source_info`: Originating client identifier and session metadata.
  // - `actions`: The protobuf containing actions to execute.
  // - `on_complete`: Fired when actions finish executing (or on failure).
  // - `tab_id`: Optional target tab ID to inject into actions lacking one.
  ActorActionsRunner(Profile& profile,
                     TaskSourceInfo source_info,
                     optimization_guide::proto::Actions actions,
                     base::OnceClosure on_complete,
                     int32_t tab_id = 0);
  ActorActionsRunner(const ActorActionsRunner&) = delete;
  ActorActionsRunner& operator=(const ActorActionsRunner&) = delete;
  ~ActorActionsRunner();

  // Starts execution of the actions.
  void Start();

  // Returns the ActionsResult once completion callback is invoked, transferring
  // ownership.
  std::unique_ptr<optimization_guide::proto::ActionsResult> TakeResult();

  // Returns a pointer to the ActionsResult if populated.
  const optimization_guide::proto::ActionsResult* result() const;

 private:
  void StopTaskIfActive();
  void OnTaskStateChanged(ActorTask& task);
  void OnActionsPerformed(
      base::TimeTicks start_time,
      bool skip_async_observation_collection,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions> screenshot_options,
      std::vector<ActionResultWithLatencyInfo> action_results,
      TabObservationStrategy observation_strategy);
  void OnActionsResultBuilt(
      base::TimeTicks start_time,
      std::vector<ActionResultWithLatencyInfo> action_results,
      TaskId task_id,
      bool skip_async_observation_information,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>
          screenshot_collection_options,
      std::unique_ptr<optimization_guide::proto::ActionsResult> result,
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry> entry);
  void Finish(std::unique_ptr<optimization_guide::proto::ActionsResult> result);

  const raw_ref<Profile> profile_;
  const TaskSourceInfo source_info_;
  optimization_guide::proto::Actions actions_;
  base::OnceClosure on_complete_;
  const int32_t tab_id_;

  bool is_started_ = false;
  std::optional<TaskId> task_id_;
  base::WeakPtr<ActorTask> actor_task_;
  base::CallbackListSubscription task_state_changed_subscription_;
  std::unique_ptr<optimization_guide::proto::ActionsResult> result_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ActorActionsRunner> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_ACTIONS_RUNNER_H_
