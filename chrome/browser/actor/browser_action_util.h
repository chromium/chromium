// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_
#define CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_

#include <memory>
#include <vector>

#include "base/types/expected.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

// Conversion function for turning optimization_guide::proto::* types into
// ToolRequests usable by the actor framework.
// TODO(bokan): Rename to actor_proto_conversion.h|cc

namespace optimization_guide::proto {
class Action;
class Actions;
class BrowserAction;
}  // namespace optimization_guide::proto

namespace page_content_annotations {
struct FetchPageContextResult;
}  // namespace page_content_annotations

namespace actor {
class ActorTask;
class ToolRequest;

// The mime type used for screenshots.
inline constexpr std::string kMimeTypeJpeg = "image/jpeg";

// Build a ToolRequest from the provided optimization_guide Action proto. If the
// action proto doesn't provide a tab_id, and the fallback_tab parameter is
// provided (non-null), the fallback_tab will be used as the acting tab.
// However, this parameter will eventually be phased out and clients will be
// expected to always provide a tab id on each Action. Returns nullptr if the
// action is invalid.
// TODO(https://crbug.com/411462297): The client should eventually always
// provide a tab id for actions where one is needed. Remove this parameter when
// that's done.
std::unique_ptr<ToolRequest> CreateToolRequest(
    const optimization_guide::proto::Action& action,
    tabs::TabInterface* deprecated_fallback_tab);

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
void BuildActionsResultWithObservations(
    content::BrowserContext& browser_context,
    mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action,
    const ActorTask& task,
    base::OnceCallback<void(
        std::unique_ptr<optimization_guide::proto::ActionsResult>)> callback);

optimization_guide::proto::ActionsResult BuildErrorActionsResult(
    mojom::ActionResultCode result_code,
    std::optional<size_t> index_of_failed_action);

// Builds a vector of ToolRequests usable for ActorKeyedService::ActInFocusedTab
// out of the given proto::BrowserAction proto.
// TODO(https://crbug.com/411462297): Remove this once the BrowserAction path is
// removed.
BuildToolRequestResult BuildToolRequest(
    const optimization_guide::proto::BrowserAction& actions,
    tabs::TabInterface* deprecated_fallback_tab);

// Converts a FetchPageContext result to a TabObservation proto. Note that this
// does not fill in the (tab) `id` field on the proto, the caller is responsible
// for that.
optimization_guide::proto::TabObservation ConvertToTabObservation(
    const page_content_annotations::FetchPageContextResult&
        page_context_result);

// Builds the BrowserActionResult proto from the output of a call to the
// ActorKeyedService::ActInFocusedTab API.
// TODO(https://crbug.com/411462297): Remove this once the BrowserAction path is
// removed.
optimization_guide::proto::BrowserActionResult BuildBrowserActionResult(
    mojom::ActionResultCode result_code,
    int32_t tab_id);

std::string ToBase64(const optimization_guide::proto::BrowserAction& actions);
std::string ToBase64(const optimization_guide::proto::Actions& actions);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_BROWSER_ACTION_UTIL_H_
