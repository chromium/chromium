// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_PROTO_WRAPPER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_PROTO_WRAPPER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/task_id.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"

namespace actor {

class ActorKeyedService;
struct ObservationResult;
class TabObservationController;

// Wraps `ActorKeyedService` to convert and execute an
// `optimization_guide::proto::Actions` request and collect post-action tab and
// window observations into an `optimization_guide::proto::ActionsResult`.
class ActorKeyedServiceProtoWrapper {
 public:
  using PerformActionsCallback =
      base::OnceCallback<void(optimization_guide::proto::ActionsResult)>;

  explicit ActorKeyedServiceProtoWrapper(ActorKeyedService* actor_service);
  ActorKeyedServiceProtoWrapper(const ActorKeyedServiceProtoWrapper&) = delete;
  ActorKeyedServiceProtoWrapper& operator=(
      const ActorKeyedServiceProtoWrapper&) = delete;
  ~ActorKeyedServiceProtoWrapper();

  void PerformActions(const optimization_guide::proto::Actions& actions,
                      PerformActionsCallback callback);

 private:
  friend class ActorKeyedServiceProtoWrapperTest;

  static optimization_guide::proto::ActionsResult BuildActionsResult(
      base::TimeTicks start_time,
      const std::vector<ActionResultWithLatencyInfo>& action_results,
      ObservationResult& observation_result);

  void OnPerformActionsFinished(
      PerformActionsCallback callback,
      TaskId task_id,
      base::TimeTicks start_time,
      bool skip_async_observation_information,
      std::optional<page_content_annotations::ScreenshotOptions::
                        ScreenshotCollectionOptions>
          screenshot_collection_options,
      std::vector<ActionResultWithLatencyInfo> action_results,
      TabObservationStrategy observation_strategy);

  void OnTabObservationComplete(
      PerformActionsCallback callback,
      base::TimeTicks start_time,
      std::vector<ActionResultWithLatencyInfo> action_results,
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
      TabObservationController* controller_ptr,
      std::unique_ptr<ObservationResult> result);

  // ActorKeyedService outlives this wrapper.
  raw_ptr<ActorKeyedService> actor_service_;
  std::vector<std::unique_ptr<TabObservationController>>
      observation_controllers_;
  base::WeakPtrFactory<ActorKeyedServiceProtoWrapper> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_PROTO_WRAPPER_H_
