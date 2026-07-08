// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace affiliations {
class DomainRelationChecker;
}  // namespace affiliations

namespace actor {

// A tool that attempts to retrieve a one-time password (OTP) and fill it into
// the specified fields on the page. (One field or many smaller ones.)
// If this is part of a sign-in flow, set `for_signin` to true.
class AttemptOtpFillingTool : public Tool {
 public:
  AttemptOtpFillingTool(TaskId task_id,
                        ToolDelegate& tool_delegate,
                        tabs::TabHandle tab_handle,
                        std::vector<PageTarget> trigger_fields,
                        bool for_signin);
  ~AttemptOtpFillingTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolCallback callback) override;

  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;

  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  void OnGmailOtpOptInResponse(ToolCallback callback,
                               webui::mojom::GmailOtpOptInResultPtr response);
  void OnOtpRetrieved(
      ToolCallback callback,
      base::expected<std::string, one_time_tokens::OneTimeTokenRetrievalError>
          result);
  void OnOtpFilled(ToolCallback callback, bool success);
  void OnActorLoginFlowChecked(ToolCallback callback, bool is_actor_login);

  void LogJournalEvent(std::string_view event_name,
                       std::vector<mojom::JournalDetailsPtr> journal_details);

  tabs::TabHandle tab_handle_;
  std::vector<PageTarget> trigger_fields_;
  std::vector<autofill::FieldGlobalId> trigger_field_ids_;
  bool for_signin_;

  // `DomainRelationChecker` finds relationship between origins (exactly the
  // same, affiliated, ePSL match, weak match, no match). used to determine if
  // an OTP form is related to the last actor login flow.
  std::unique_ptr<affiliations::DomainRelationChecker> domain_relation_checker_;

  base::WeakPtrFactory<AttemptOtpFillingTool> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_
