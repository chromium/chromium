// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/autofill_selection_dialog_event_handler.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "components/autofill/core/browser/actor/actor_form_filling_service.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
class AutofillClient;
}

namespace actor {

class ActorSurface;

class AttemptFormFillingTool : public Tool,
                               public AutofillSelectionDialogEventHandler {
 public:
  // If `credit_card_opaque_token` is a non-empty string, the data to fill is
  // queried via
  // `ActorFormFillingService::RetrieveSuggestionForCreditCardOpaqueToken` for
  // the specified token. The user does not get to choose the data to fill.
  AttemptFormFillingTool(
      TaskId task_id,
      ToolDelegate& tool_delegate,
      ActorSurface& actor_surface,
      std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests,
      std::string credit_card_opaque_token,
      bool enqueued_click);
  ~AttemptFormFillingTool() override;

  void Invoke(ToolCallback callback) override;
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  ActorSurfaceHandle GetTargetActorSurface() const override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;

  const std::string& credit_card_opaque_token() const {
    return credit_card_opaque_token_;
  }

  // AutofillSelectionDialogEventHandler implementation.
  bool OnFormPresented(
      webui::mojom::AutofillSuggestionDialogOnFormPresentedParamsPtr params)
      override;
  void OnFormPreviewChanged(
      webui::mojom::AutofillSuggestionDialogOnFormPreviewChangedParamsPtr
          params) override;
  bool OnFormConfirmed(
      webui::mojom::AutofillSuggestionDialogOnFormConfirmedParamsPtr params)
      override;

 private:
  void OnSuggestionsRetrieved(
      ToolCallback invoke_callback,
      base::expected<std::vector<autofill::ActorFormFillingRequest>,
                     autofill::ActorFormFillingError> suggestions_result);
  void OnSuggestionsSelected(
      ToolCallback invoke_callback,
      webui::mojom::SelectAutofillSuggestionsDialogResponsePtr);
  // Directly fills the suggestions into the form without asking the user to
  // select a suggestion. Called when
  // `actor::switches::kAttemptFormFillingToolSkipsUI` is passed (for testing
  // purposes) or when `credit_card_opaque_token_` is non-empty.
  void DirectlyFillSuggestions(
      ToolCallback invoke_callback,
      const std::vector<autofill::ActorFormFillingRequest>& requests);
  autofill::AutofillClient* GetAutofillClient();
  ActorSurfaceHandle actor_surface_handle_;
  std::vector<AttemptFormFillingToolRequest::FormFillingRequest>
      tool_fill_requests_;
  // An optional opaque token to retrieve and fill a credit card suggestion.
  std::string credit_card_opaque_token_;
  // Set to true if a click has already been enqueued for the target field of
  // this request. This is propagated from the request to prevent infinite loops
  // of click delegation.
  bool enqueued_click_ = false;
  // Fill requests as determined by the ActorFormFillingService. Initially
  // populated in TimeOfUseValidation and then updated in OnSuggestionsRetrieved
  // to reflect the actual requests returned by the service, which may have
  // been split.
  std::vector<autofill::ActorFormFillingService::FillRequest>
      service_fill_requests_;
  base::WeakPtrFactory<AttemptFormFillingTool> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_H_
