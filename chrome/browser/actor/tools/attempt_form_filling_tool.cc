// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_form_filling_tool.h"

#include <sstream>

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_metrics.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/autofill/actor/actor_form_filling_service.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_logging.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace actor {

using autofill::FieldGlobalId;

namespace {

mojom::ActionResultPtr FromServiceError(autofill::ActorFormFillingError error) {
  switch (error) {
    case autofill::ActorFormFillingError::kAutofillNotAvailable:
      return MakeResult(
          mojom::ActionResultCode::kFormFillingAutofillUnavailable,
          /*requires_page_stabilization=*/false, "Autofill is not available.");
    case autofill::ActorFormFillingError::kNoSuggestions:
      return MakeResult(
          mojom::ActionResultCode::kFormFillingNoSuggestionsAvailable,
          /*requires_page_stabilization=*/false,
          "No autofill suggestions available for the fields.");
    case autofill::ActorFormFillingError::kNoForm:
      return MakeResult(
          mojom::ActionResultCode::kObservedTargetElementDestroyed,
          /*requires_page_stabilization=*/false,
          "The form was not found or has changed.");
    case autofill::ActorFormFillingError::kOther:
      return MakeResult(
          mojom::ActionResultCode::kFormFillingUnknownAutofillError,
          /*requires_page_stabilization=*/false,
          "An unknown error occurred in the form filling service.");
  }
  NOTREACHED();
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

void AttemptFormFillingTool::Invoke(ToolCallback callback) {
  // `service_fill_requests_` must have been set by TimeOfUseValidation() or
  // otherwise an error was returned by TimeOfUseValidation().
  CHECK(!service_fill_requests_.empty());

  form_fill_metrics::RecordOnInvokeMetrics();

  if (tabs::TabInterface* tab = GetTargetTab().Get()) {
    journal().Log(
        JournalURL(), task_id(), "AttemptFormFillingTool::Invoke",
        JournalDetailsBuilder().Add("requests", tool_fill_requests_).Build());

    tool_delegate().GetActorFormFillingService().GetSuggestions(
        *tab, service_fill_requests_,
        base::BindOnce(&AttemptFormFillingTool::OnSuggestionsRetrieved,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void AttemptFormFillingTool::Validate(ToolCallback callback) {
  std::move(callback).Run(MakeOkResult());
}

mojom::ActionResultPtr AttemptFormFillingTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  tabs::TabInterface* tab = GetTargetTab().Get();
  // TimeOfUseValidation() is only invoked once, so `service_fill_requests_`
  // will be empty since AttemptFormFillingTool construction.
  CHECK(service_fill_requests_.empty());

  if (!last_observation) {
    return MakeResult(mojom::ActionResultCode::kFormFillingNoLastTabObservation,
                      /*requires_page_stabilization=*/false,
                      "AttemptFormFillingTool: last tab observation is null.");
  }

  for (const auto& request : tool_fill_requests_) {
    std::vector<FieldGlobalId> field_ids;
    for (const auto& trigger_field : request.trigger_fields) {
      autofill::FieldGlobalId current_field_id =
          GetFieldIdFromPageTarget(last_observation, tab, trigger_field);
      if (!current_field_id) {
        return MakeResult(mojom::ActionResultCode::kFormFillingFieldNotFound,
                          /*requires_page_stabilization=*/false,
                          "Trigger field not found.");
      }
      field_ids.push_back(current_field_id);
    }
    if (field_ids.empty()) {
      return MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                        /*requires_page_stabilization=*/false,
                        "At least one trigger field must be provided.");
    }
    service_fill_requests_.emplace_back(request.requested_data,
                                        std::move(field_ids));
  }

  if (service_fill_requests_.empty()) {
    return MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                      /*requires_page_stabilization=*/false,
                      "At least one form filling requests must be provided.");
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

void AttemptFormFillingTool::UpdateTaskBeforeInvoke(
    ActorTask& task,
    ToolCallback callback) const {
  task.AddTab(tab_handle_, /*stop_task_on_detach=*/true, std::move(callback));
}

void AttemptFormFillingTool::OnSuggestionsRetrieved(
    ToolCallback invoke_callback,
    base::expected<std::vector<autofill::ActorFormFillingRequest>,
                   autofill::ActorFormFillingError> suggestions_result) {
  const int suggestions_count =
      suggestions_result.has_value() ? suggestions_result.value().size() : 0;
  form_fill_metrics::RecordOnSuggestionsRetrievedMetrics(suggestions_count);

  if (!suggestions_result.has_value()) {
    std::move(invoke_callback)
        .Run(FromServiceError(suggestions_result.error()));
    return;
  }

  // Update service_fill_requests_ to reflect the actual requests returned by
  // the service, which may have been split.
  service_fill_requests_.clear();
  for (const autofill::ActorFormFillingRequest& request :
       suggestions_result.value()) {
    service_fill_requests_.emplace_back(request.requested_data,
                                        std::vector<FieldGlobalId>{});
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAttemptFormFillingToolSkipsUI)) {
    SimulateRequestToShowAutofillSuggestions(std::move(invoke_callback),
                                             suggestions_result.value());
    return;
  }

  tool_delegate().RequestToShowAutofillSuggestions(
      std::move(*suggestions_result), weak_factory_.GetWeakPtr(),
      base::BindOnce(&AttemptFormFillingTool::OnSuggestionsSelected,
                     weak_factory_.GetWeakPtr(), std::move(invoke_callback)));
}

void AttemptFormFillingTool::SimulateRequestToShowAutofillSuggestions(
    ToolCallback invoke_callback,
    std::vector<autofill::ActorFormFillingRequest> requests) {
  // In the simulation of asking the user to pick suggestions, we just choose
  // the first suggestion for each form section.
  std::vector<webui::mojom::FormFillingResponsePtr> accepted_suggestions =
      base::ToVector(
          requests, [](const autofill::ActorFormFillingRequest& request) {
            std::string chosen_suggestion_id;
            if (!request.suggestions.empty()) {
              chosen_suggestion_id =
                  base::NumberToString(request.suggestions[0].id.value());
            }
            return webui::mojom::FormFillingResponse::New(chosen_suggestion_id);
          });

  auto dialog_response =
      webui::mojom::SelectAutofillSuggestionsDialogResponse::New();
  dialog_response->task_id = task_id().value();
  dialog_response->result =
      webui::mojom::SelectAutofillSuggestionsDialogResult::
          NewSelectedSuggestions(std::move(accepted_suggestions));
  OnSuggestionsSelected(std::move(invoke_callback), std::move(dialog_response));
}

void AttemptFormFillingTool::OnSuggestionsSelected(
    ToolCallback invoke_callback,
    webui::mojom::SelectAutofillSuggestionsDialogResponsePtr dialog_response) {
  if (dialog_response->result->is_error_reason()) {
    std::move(invoke_callback)
        .Run(MakeResult(mojom::ActionResultCode::kFormFillingDialogError,
                        /*requires_page_stabilization=*/false,
                        "Showing suggestions to the user failed."));
    return;
  }
  if (dialog_response->result->get_selected_suggestions().empty()) {
    std::move(invoke_callback)
        .Run(MakeResult(mojom::ActionResultCode::kFormFillingDialogError,
                        /*requires_page_stabilization=*/false,
                        "Dialog response contains no selected suggestions."));
    return;
  }
  std::vector<autofill::ActorFormFillingSelection> selection_response;
  for (const auto& response :
       dialog_response->result->get_selected_suggestions()) {
    uint32_t id = 0;
    if (!base::StringToUint(response->selected_suggestion_id, &id)) {
      std::move(invoke_callback)
          .Run(MakeResult(
              mojom::ActionResultCode::kFormFillingInvalidSuggestionId,
              /*requires_page_stabilization=*/false,
              "Invalid suggestion ID received."));
      return;
    }
    autofill::ActorFormFillingSelection selection;
    selection.selected_suggestion_id = autofill::ActorSuggestionId(id);
    selection_response.push_back(std::move(selection));
  }
  tabs::TabInterface* tab = GetTargetTab().Get();
  if (!tab) {
    std::move(invoke_callback)
        .Run(MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }
  tool_delegate().GetActorFormFillingService().FillSuggestions(
      *tab, std::move(selection_response),
      base::BindOnce([](base::expected<void, autofill::ActorFormFillingError>
                            result) {
        return result.has_value() ? MakeOkResult()
                                  : FromServiceError(result.error());
      }).Then(std::move(invoke_callback)));
}

bool AttemptFormFillingTool::OnFormPresented(
    webui::mojom::AutofillSuggestionDialogOnFormPresentedParamsPtr params) {
  tabs::TabInterface* tab = GetTargetTab().Get();
  if (!tab) {
    return true;
  }
  if (params->form_filling_request_index < 0) {
    return false;
  }
  size_t request_index =
      static_cast<size_t>(params->form_filling_request_index);
  if (request_index >= service_fill_requests_.size()) {
    return false;
  }

  form_fill_metrics::RecordOnSuggestionPresentedMetrics(
      /*is_first=*/request_index == 0,
      service_fill_requests_[request_index].first);
  tool_delegate().GetActorFormFillingService().ScrollToForm(*tab,
                                                            request_index);
  return true;
}

void AttemptFormFillingTool::OnFormPreviewChanged(
    webui::mojom::AutofillSuggestionDialogOnFormPreviewChangedParamsPtr
        params) {
  tabs::TabInterface* tab = GetTargetTab().Get();
  if (!tab) {
    return;
  }
  if (params->response) {
    uint32_t id = 0;
    if (base::StringToUint(params->response->selected_suggestion_id, &id)) {
      tool_delegate().GetActorFormFillingService().PreviewForm(
          *tab, params->form_filling_request_index,
          autofill::ActorSuggestionId(id));
    }
  } else {
    tool_delegate().GetActorFormFillingService().ClearFormPreview(
        *tab, params->form_filling_request_index);
  }
}

bool AttemptFormFillingTool::OnFormConfirmed(
    webui::mojom::AutofillSuggestionDialogOnFormConfirmedParamsPtr params) {
  tabs::TabInterface* tab = GetTargetTab().Get();
  if (!tab) {
    return true;
  }
  if (params->form_filling_request_index < 0) {
    return false;
  }
  size_t request_index =
      static_cast<size_t>(params->form_filling_request_index);
  if (request_index >= service_fill_requests_.size()) {
    return false;
  }
  uint32_t id = 0;
  if (!base::StringToUint(params->response->selected_suggestion_id, &id)) {
    return false;
  }

  form_fill_metrics::RecordOnSuggestionConfirmedMetrics(
      /*is_last=*/request_index == service_fill_requests_.size() - 1,
      service_fill_requests_[request_index].first);
  autofill::ActorFormFillingSelection selection;
  selection.selected_suggestion_id = autofill::ActorSuggestionId(id);
  tool_delegate().GetActorFormFillingService().FillForm(
      *tab, params->form_filling_request_index, std::move(selection));
  return true;
}

}  // namespace actor
