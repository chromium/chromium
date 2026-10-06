// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion_service.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/selection/features.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "components/version_info/version_info.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"

namespace selection {

namespace {

namespace proto = ::optimization_guide::proto;

// Converts `aoi` into its proto representation, excluding screenshot data.
proto::AreaOfInterest ToProtoAreaOfInterest(const AreaOfInterest& aoi) {
  proto::AreaOfInterest proto_aoi;
  if (const gfx::Rect* rect = std::get_if<gfx::Rect>(&aoi.bounds)) {
    proto::Rectangle& selection = *proto_aoi.mutable_selection();
    selection.set_x(rect->x());
    selection.set_y(rect->y());
    selection.set_width(rect->width());
    selection.set_height(rect->height());
  }
  *proto_aoi.mutable_annotated_page_content() = aoi.apc;
  return proto_aoi;
}

// Builds a request proto for `aoi` and `tools`, excluding screenshot data.
// Returns `std::nullopt` if no tools supporting server suggestions are
// registered.
std::optional<proto::SmartSelectionSuggestionsRequest>
BuildServerSuggestionsRequest(
    const AreaOfInterest& aoi,
    const base::flat_map<SuggestionTool::ToolId, raw_ptr<SuggestionTool>>&
        tools) {
  proto::SmartSelectionSuggestionsRequest request;
  proto::SmartSelectionClientCapabilities& capabilities =
      *request.mutable_client_capabilities();
  for (const auto& [tool_id, tool] : tools) {
    if (!tool->SupportsServerSuggestions()) {
      continue;
    }
    proto::SmartSelectionToolWithCapabilities& tool_cap =
        *capabilities.add_available_tools();
    tool_cap.set_tool(tool_id);
    // TODO(crbug.com/561489586): Populate tool capabilities.
  }
  if (capabilities.available_tools().empty()) {
    return std::nullopt;
  }

  if (g_browser_process) {
    capabilities.set_locale(g_browser_process->GetApplicationLocale());
  }
  capabilities.set_platform(optimization_guide::GetChromePlatform());
  capabilities.set_chrome_version(
      std::string(version_info::GetVersionNumber()));

  *request.add_areas_of_interest() = ToProtoAreaOfInterest(aoi);
  return request;
}

// Parses `result` and invokes the corresponding `tools` to create
// `Suggestion`s.
std::vector<std::unique_ptr<Suggestion>> ExtractSuggestionsFromResponse(
    const AreaOfInterest& aoi,
    const optimization_guide::OptimizationGuideModelExecutionResult& result,
    const base::flat_map<SuggestionTool::ToolId, raw_ptr<SuggestionTool>>&
        tools) {
  if (!result.response.has_value()) {
    return {};
  }

  const std::optional<proto::SmartSelectionSuggestionsResponse> response =
      optimization_guide::ParsedAnyMetadata<
          proto::SmartSelectionSuggestionsResponse>(result.response.value());
  if (!response.has_value()) {
    return {};
  }

  std::vector<std::unique_ptr<Suggestion>> suggestions;
  suggestions.reserve(response->suggestions().size());
  for (const proto::SmartSelectionSuggestion& server_suggestion :
       response->suggestions()) {
    if (const raw_ptr<SuggestionTool>* tool =
            base::FindOrNull(tools, server_suggestion.tool());
        tool && (*tool)->SupportsServerSuggestions()) {
      if (std::unique_ptr<Suggestion> suggestion =
              (*tool)->CreateSuggestion(aoi, server_suggestion)) {
        suggestions.emplace_back(std::move(suggestion));
      }
    }
  }
  return suggestions;
}

}  // namespace

DEFINE_USER_DATA(SuggestionService);

struct SuggestionService::ActiveRequest
    : public base::RefCounted<ActiveRequest> {
  ActiveRequest(const AreaOfInterest& aoi,
                size_t num_tools,
                SuggestionsCallback cb)
      : aoi(aoi), remaining_tools(num_tools), callback(std::move(cb)) {}

  bool complete() const {
    return remaining_tools == 0 && !is_awaiting_server_suggestions;
  }

  const AreaOfInterest aoi;
  size_t remaining_tools;
  SuggestionsCallback callback;
  bool in_synchronous_dispatch = true;
  bool has_synchronous_response = false;
  bool is_awaiting_server_suggestions = false;
  std::vector<std::unique_ptr<Suggestion>> synchronous_suggestions;

 private:
  friend class base::RefCounted<ActiveRequest>;
  ~ActiveRequest() = default;
};

// static
SuggestionService* SuggestionService::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
SuggestionService* SuggestionService::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(tab_web_contents);
  return From(tab);
}

SuggestionService::SuggestionService(
    tabs::TabInterface* tab,
    optimization_guide::RemoteModelExecutor* remote_model_executor)
    : tab_(CHECK_DEREF(tab)),
      remote_model_executor_(remote_model_executor),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

SuggestionService::~SuggestionService() {
  tools_.clear();
}

void SuggestionService::RegisterTool(SuggestionTool* tool) {
  if (!tool) {
    return;
  }
  const SuggestionTool::ToolId tool_id = tool->GetToolId();
  CHECK_NE(tool_id, SuggestionTool::ToolId::SMART_SELECTION_TOOL_UNSPECIFIED);
  CHECK(tools_.try_emplace(tool_id, tool).second);
}

void SuggestionService::UnregisterTool(SuggestionTool* tool) {
  if (!tool) {
    return;
  }
  if (auto it = tools_.find(tool->GetToolId());
      it != tools_.end() && it->second == tool) {
    tools_.erase(it);
  }
}

void SuggestionService::UpdateScreenContent(
    const SkBitmap& screenshot,
    const optimization_guide::proto::AnnotatedPageContent& apc) {}

void SuggestionService::RequestSuggestions(const AreaOfInterest& processed_area,
                                           SuggestionsCallback callback) {
  if (tools_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<std::unique_ptr<Suggestion>>(),
                       /*complete=*/true));
    return;
  }

  auto active_request = base::MakeRefCounted<ActiveRequest>(
      processed_area, tools_.size(), std::move(callback));
  const base::flat_map<SuggestionTool::ToolId, raw_ptr<SuggestionTool>>
      tools_snapshot = tools_;
  for (const auto& [tool_id, tool] : tools_snapshot) {
    tool->RequestSuggestions(
        active_request->aoi,
        base::BindRepeating(&SuggestionService::OnToolSuggestions,
                            weak_factory_.GetWeakPtr(), active_request,
                            base::OwnedRef(false)));
  }

  active_request->in_synchronous_dispatch = false;
  RequestServerSuggestions(active_request);

  if (active_request->has_synchronous_response) {
    active_request->callback.Run(
        std::exchange(active_request->synchronous_suggestions, {}),
        active_request->complete());
  }
}

void SuggestionService::RequestServerSuggestions(
    scoped_refptr<ActiveRequest> active_request) {
  if (!remote_model_executor_ ||
      !base::FeatureList::IsEnabled(kSmartSelectionServerSuggestions)) {
    return;
  }

  std::optional<proto::SmartSelectionSuggestionsRequest> request =
      BuildServerSuggestionsRequest(active_request->aoi, tools_);
  if (!request.has_value()) {
    return;
  }

  active_request->is_awaiting_server_suggestions = true;
  const SkBitmap& screenshot = active_request->aoi.screenshot;
  // TODO(crbug.com/561489586): Investigate alternative image encoding and
  // compression mechanisms.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&gfx::PNGCodec::EncodeBGRASkBitmap, screenshot,
                     /*discard_transparency=*/false),
      base::BindOnce(&SuggestionService::SendServerSuggestionsRequest,
                     weak_factory_.GetWeakPtr(), std::move(active_request),
                     *std::move(request)));
}

void SuggestionService::SendServerSuggestionsRequest(
    scoped_refptr<ActiveRequest> active_request,
    proto::SmartSelectionSuggestionsRequest request,
    std::optional<std::vector<uint8_t>> png_bytes) {
  if (png_bytes.has_value()) {
    CHECK_EQ(request.areas_of_interest_size(), 1);
    proto::AreaOfInterest& proto_aoi = *request.mutable_areas_of_interest(0);
    proto_aoi.set_image_bytes(base::as_string_view(*png_bytes));
    proto_aoi.set_mime_type("image/png");
  }

  // TODO(crbug.com/561489586): Consider adding logging.
  remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kSmartSelectionSuggestions,
      request, {.execution_timeout = kSmartSelectionServerTimeout.Get()},
      base::BindOnce(&SuggestionService::OnServerSuggestions,
                     weak_factory_.GetWeakPtr(), std::move(active_request)));
}

void SuggestionService::OnServerSuggestions(
    scoped_refptr<ActiveRequest> active_request,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!active_request->is_awaiting_server_suggestions) {
    return;
  }
  active_request->is_awaiting_server_suggestions = false;

  active_request->callback.Run(
      ExtractSuggestionsFromResponse(active_request->aoi, result, tools_),
      active_request->complete());
}

void SuggestionService::OnToolSuggestions(
    scoped_refptr<ActiveRequest> active_request,
    bool& tool_completed,
    std::vector<std::unique_ptr<Suggestion>> suggestions,
    bool complete) {
  if (complete && !tool_completed) {
    tool_completed = true;
    active_request->remaining_tools--;
  }
  if (active_request->in_synchronous_dispatch) {
    active_request->has_synchronous_response = true;
    base::Extend(active_request->synchronous_suggestions,
                 std::move(suggestions));
    return;
  }
  active_request->callback.Run(std::move(suggestions),
                               active_request->complete());
}

}  // namespace selection
