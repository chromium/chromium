// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class AttemptFormFillingTool : public Tool {
 public:
  AttemptFormFillingTool(
      TaskId task_id,
      ToolDelegate& tool_delegate,
      tabs::TabInterface& tab,
      std::vector<AttemptFormFillingToolRequest::FormFillingRequest> requests);
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
  tabs::TabHandle GetTargetTab() const override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;

 private:
  void OnSuggestionsRetrieved(
      ToolCallback invoke_callabck,
      base::expected<std::vector<autofill::ActorFormFillingRequest>,
                     autofill::ActorFormFillingError> suggestions_result);
  void OnSuggestionsSelected(
      ToolCallback invoke_callback,
      webui::mojom::SelectAutofillSuggestionsDialogResponsePtr);
  // Called by OnSuggestionsRetrieved if
  // actor::switches::kAttemptFormFillingToolSkipsUI is passed as a command line
  // switch. In this case, the user does not get to select the data that should
  // be filled. Instead of that the first suggestion is chosen. This is only
  // useful for testing purposes.
  void SimulateRequestToShowAutofillSuggestions(
      ToolCallback invoke_callback,
      std::vector<autofill::ActorFormFillingRequest> requests);
  tabs::TabHandle tab_handle_;
  std::vector<AttemptFormFillingToolRequest::FormFillingRequest>
      tool_fill_requests_;
  std::vector<std::pair<AttemptFormFillingToolRequest::RequestedData,
                        std::vector<autofill::FieldGlobalId>>>
      service_fill_requests_;
  base::WeakPtrFactory<AttemptFormFillingTool> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_H_
