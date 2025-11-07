// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool.h"

#include <sstream>

#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/autofill/glic/actor_form_filling_service.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace actor {

using autofill::FieldGlobalId;
using content::RenderFrameHost;
using content::WebContents;
using optimization_guide::TargetNodeInfo;
using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::FormFillingRequest;

namespace {

std::optional<optimization_guide::TargetNodeInfo>
FindLastObservedNodeForActionTarget(
    const optimization_guide::proto::AnnotatedPageContent* apc,
    const PageTarget& target) {
  return std::visit(
      absl::Overload{
          [&](const DomNode& node) {
            return FindLastObservedNodeForActionTargetId(apc, node);
          },
          [&](const gfx::Point& point) {
            return FindLastObservedNodeForActionTargetPoint(apc, point);
          },
      },
      target);
}

FieldGlobalId GetFieldIdFromPageTarget(
    const optimization_guide::proto::AnnotatedPageContent* last_observation,
    tabs::TabInterface* tab,
    const PageTarget& target) {
  if (std::optional<TargetNodeInfo> node_info =
          FindLastObservedNodeForActionTarget(last_observation, target)) {
    if (WebContents* web_contents = tab->GetContents()) {
      if (RenderFrameHost* rfh =
              optimization_guide::GetRenderFrameForDocumentIdentifier(
                  *web_contents,
                  node_info->document_identifier.serialized_token())) {
        return FieldGlobalId(
            autofill::LocalFrameToken(rfh->GetFrameToken().value()),
            autofill::FieldRendererId(node_info->node->content_attributes()
                                          .common_ancestor_dom_node_id()));
      }
    }
  }
  return {};
}

optimization_guide::proto::FormFillingRequest_RequestedData
ConvertRequestedDataToProtoEnum(
    AttemptFormFillingToolRequest::RequestedData enum_value) {
  switch (enum_value) {
    case AttemptFormFillingToolRequest::RequestedData::kUnknown:
      return optimization_guide::proto::
          FormFillingRequest_RequestedData_REQUESTED_DATA_UNKNOWN;
    case actor::AttemptFormFillingToolRequest::RequestedData::kAddress:
      return optimization_guide::proto::
          FormFillingRequest_RequestedData_ADDRESS;
    case actor::AttemptFormFillingToolRequest::RequestedData::kBillingAddress:
      return optimization_guide::proto::
          FormFillingRequest_RequestedData_BILLING_ADDRESS;
    case actor::AttemptFormFillingToolRequest::RequestedData::kShippingAddress:
      return optimization_guide::proto::
          FormFillingRequest_RequestedData_SHIPPING_ADDRESS;
    case actor::AttemptFormFillingToolRequest::RequestedData::kWorkAddress:
      return optimization_guide::proto::
          FormFillingRequest_RequestedData_WORK_ADDRESS;
    case actor::AttemptFormFillingToolRequest::RequestedData::kHomeAddress:
      return optimization_guide::proto::
          FormFillingRequest_RequestedData_HOME_ADDRESS;
    case actor::AttemptFormFillingToolRequest::RequestedData::kCreditCard:
      return optimization_guide::proto::
          FormFillingRequest_RequestedData_CREDIT_CARD;
  }
}

}  // namespace

AttemptFormFillingTool::AttemptFormFillingTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    tabs::TabInterface& tab,
    std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab.GetHandle()),
      tool_fill_requests_(std::move(requests)) {}

AttemptFormFillingTool::~AttemptFormFillingTool() = default;

void AttemptFormFillingTool::Invoke(InvokeCallback callback) {
  // `service_fill_requests_` must have been set by TimeOfUseValidation() or
  // otherwise an error was returned by TimeOfUseValidation().
  CHECK(!service_fill_requests_.empty());
  if (tabs::TabInterface* tab = GetTargetTab().Get()) {
    journal().Log(
        JournalURL(), task_id(), "AttemptFormFillingTool::Invoke",
        JournalDetailsBuilder().Add("requests", tool_fill_requests_).Build());

    tool_delegate().GetActorFormFillingService().GetSuggestions(
        *tab,
        base::ToVector(std::move(service_fill_requests_),
                       [](auto&& entry) {
                         return std::make_pair(
                             // TODO(crbug.com/452065032): Refactor
                             // ActorFormFilingService to use the RequestedData
                             // from AttemptFormFillingToolRequest, then avoid
                             // converting here.
                             ConvertRequestedDataToProtoEnum(entry.first),
                             std::move(entry.second));
                       }),
        base::BindOnce(&AttemptFormFillingTool::OnSuggestionsRetrieved,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void AttemptFormFillingTool::Validate(ValidateCallback callback) {
  std::move(callback).Run(MakeOkResult());
}

mojom::ActionResultPtr AttemptFormFillingTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  tabs::TabInterface* tab = GetTargetTab().Get();
  // TimeOfUseValidation() is only invoked once, so `service_fill_requests_`
  // will be empty since AttemptFormFillingTool construction.
  CHECK(service_fill_requests_.empty());

  if (!last_observation) {
    ACTOR_LOG() << "APC was null during TimeOfUseValidation.";
    // TODO(crbug.com/454017250): Use form filling specific ActionResultCode
    // values.
    return MakeErrorResult();
  }

  for (const auto& request : tool_fill_requests_) {
    std::vector<FieldGlobalId> field_ids;
    for (const auto& trigger_field : request.trigger_fields) {
      autofill::FieldGlobalId current_field_id =
          GetFieldIdFromPageTarget(last_observation, tab, trigger_field);
      if (!current_field_id) {
        // TODO(crbug.com/454017250): Use form filling specific ActionResultCode
        // values.
        return MakeErrorResult();
      }
      field_ids.push_back(current_field_id);
    }
    if (field_ids.empty()) {
      // TODO(crbug.com/454017250): Use form filling specific ActionResultCode
      // values.
      return MakeErrorResult();
    }
    service_fill_requests_.emplace_back(request.requested_data,
                                        std::move(field_ids));
  }

  if (service_fill_requests_.empty()) {
    // TODO(crbug.com/454017250): Use form filling specific ActionResultCode
    // values.
    return MakeErrorResult();
  }
  return MakeOkResult();
}

std::string AttemptFormFillingTool::DebugString() const {
  std::stringstream out;
  out << "AttemptFormFillingTool([";
  for (const auto& form_filling_request : tool_fill_requests_) {
    out << static_cast<int>(form_filling_request.requested_data) << ", ";
  }
  out << "])";
  return out.str();
}

std::string AttemptFormFillingTool::JournalEvent() const {
  return "AttemptFormFillingTool";
}

std::unique_ptr<ObservationDelayController>
AttemptFormFillingTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  content::RenderFrameHost* rfh =
      tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

tabs::TabHandle AttemptFormFillingTool::GetTargetTab() const {
  return tab_handle_;
}

void AttemptFormFillingTool::OnSuggestionsRetrieved(
    InvokeCallback invoke_callback,
    base::expected<std::vector<autofill::ActorFormFillingRequest>,
                   autofill::ActorFormFillingError> suggestions_result) {
  if (!suggestions_result.has_value()) {
    // TODO(crbug.com/454017250): Use form filling specific ActionResultCode
    // values.
    std::move(invoke_callback)
        .Run(MakeResult(
            mojom::ActionResultCode::kError,
            /*requires_page_stabilization=*/false,
            "No suggestions returned by the actor form filling service."));
    return;
  }

  tool_delegate().RequestToShowAutofillSuggestions(
      std::move(*suggestions_result),
      base::BindOnce(&AttemptFormFillingTool::OnSuggestionsSelected,
                     weak_factory_.GetWeakPtr(), std::move(invoke_callback)));
}

void AttemptFormFillingTool::OnSuggestionsSelected(
    InvokeCallback invoke_callback,
    webui::mojom::SelectAutofillSuggestionsDialogResponsePtr dialog_response) {
  if (dialog_response->result->is_error_reason()) {
    // TODO(crbug.com/454017250): Use form filling specific ActionResultCode
    // values.
    std::move(invoke_callback).Run(MakeErrorResult());
    return;
  }
  std::vector<autofill::ActorFormFillingSelection> selection_response;
  for (const auto& response :
       dialog_response->result->get_selected_suggestions()) {
    uint32_t id = 0;
    if (!base::StringToUint(response->selected_suggestion_id, &id)) {
      // TODO(crbug.com/454017250): Use form filling specific ActionResultCode
      // values.
      std::move(invoke_callback).Run(MakeErrorResult());
      return;
    }
    autofill::ActorFormFillingSelection selection;
    selection.selected_suggestion_id = autofill::ActorSuggestionId(id);
    selection_response.push_back(std::move(selection));
  }
  auto* tab = GetTargetTab().Get();
  if (!tab) {
    std::move(invoke_callback)
        .Run(MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  tool_delegate().GetActorFormFillingService().FillSuggestions(
      *tab, std::move(selection_response),
      base::BindOnce([](base::expected<void, autofill::ActorFormFillingError>
                            result) {
        // TODO(crbug.com/454017250): Use form filling specific
        // ActionResultCode values.
        return result.has_value() ? MakeOkResult() : MakeErrorResult();
      }).Then(std::move(invoke_callback)));
}

}  // namespace actor
