// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_PROTO_CONVERSION_H_
#define CHROME_BROWSER_ACTOR_ACTOR_PROTO_CONVERSION_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/safe_ref.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

// Conversion function for turning optimization_guide::proto::* types into
// ToolRequests usable by the actor framework.
namespace content {
class BrowserContext;
}

namespace optimization_guide::proto {
class Actions;
}  // namespace optimization_guide::proto

namespace page_content_annotations {
struct FetchPageContextResult;
}  // namespace page_content_annotations

namespace actor {
class ActorTask;
class ToolRequest;

// Input type used for ActorKeyedService acting APIs, created from
// BuildToolRequest functions below. Aliased for convenience.
using ToolRequestList = std::vector<std::unique_ptr<ToolRequest>>;

// Result type returned from the BuildToolRequest functions below. Aliased for
// convenience. on failure, the error value contains the index of the action in
// the list that failed to convert.
using BuildToolRequestResult =
    base::expected<ToolRequestList, size_t /*index_of_failed_action*/>;

// Builds a vector of ToolRequests usable for ActorKeyedService::PerformActions
// out of the given proto::Actions proto. If an action failed to convert,
// returns the index of the failing action.
BuildToolRequestResult BuildToolRequest(
    const optimization_guide::proto::Actions& actions);

// Builds the ActionsResult proto from the output of a call to the
// ActorKeyedService::PerformActions API and fetches new observations for
// tabs relevant to the actions.
// TODO(bokan): Wrap the params in a struct
void BuildActionsResultWithObservations(
    content::BrowserContext& browser_context,
    base::TimeTicks start_time,
    mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action,
    std::vector<actor::ActionResultWithLatencyInfo> action_results,
    const ActorTask& task,
    bool skip_async_observation_information,
    base::OnceCallback<
        void(base::TimeTicks start_time,
             mojom::ActionResultCode result_code,
             std::optional<size_t> index_of_failed_action,
             std::vector<actor::ActionResultWithLatencyInfo> action_results,
             actor::TaskId task_id,
             bool skip_async_observation_information,
             std::unique_ptr<optimization_guide::proto::ActionsResult>,
             std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry>)>
        callback);

// For testing: when set, the callback is used to fill in the TabObservation
// using the resulting FetchPageContextResult allowing tests to verify error
// handling of the fetch.
void SetTabObservationResultOverrideForTesting(
    base::RepeatingCallback<void(
        optimization_guide::proto::TabObservation*,
        const page_content_annotations::FetchPageContextResult&)> callback);

optimization_guide::proto::ActionsResult BuildErrorActionsResult(
    mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action);

// Converts a FetchPageContext result to a TabObservation proto. Note that this
// does not fill in the (tab) `id` field on the proto, the caller is responsible
// for that.
void FillInTabObservation(
    const page_content_annotations::FetchPageContextResult& page_context_result,
    optimization_guide::proto::TabObservation& tab_observation);

// Copies script tool results in `action_results` to the input proto.
template <typename T>
void CopyScriptToolResults(
    T& proto,
    const std::vector<ActionResultWithLatencyInfo>& action_results) {
  for (size_t i = 0; i < action_results.size(); ++i) {
    const auto& response = action_results[i].result->script_tool_response;
    if (response && response->result) {
      auto* script_tool_result = proto.add_script_tool_results();
      script_tool_result->set_index_of_script_tool_action(i);
      script_tool_result->set_result(*response->result);
      script_tool_result->set_tool_name(response->name);
      script_tool_result->set_input_arguments(response->input_arguments);
      auto* tool = script_tool_result->mutable_tool();
      tool->set_name(response->tool->name);
      tool->set_description(response->tool->description);
      if (response->tool->input_schema.has_value()) {
        tool->set_input_schema(response->tool->input_schema.value());
      }
      if (response->tool->annotations) {
        tool->mutable_annotations()->set_read_only(
            response->tool->annotations->read_only);
      }
    }
  }
}

// Creates a FetchPageProgressListener that logs events to the journal.
std::unique_ptr<page_content_annotations::FetchPageProgressListener>
CreateActorJournalFetchPageProgressListener(
    base::SafeRef<AggregatedJournal> journal,
    const GURL& url,
    TaskId task_id);

std::string ToBase64(const optimization_guide::proto::Actions& actions);

std::optional<mojom::ActionResultCode> MaybeGetErrorCodeForTab(
    tabs::TabInterface* tab);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_PROTO_CONVERSION_H_
