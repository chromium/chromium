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
#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

namespace actor {

class ActorLoginFlowVerifier;

// A tool that attempts to retrieve a one-time password (OTP) and fill it into
// the specified fields on the page. (One field or many smaller ones.)
// If this is part of a sign-in flow, set `for_signin` to true.
class AttemptOtpFillingTool : public Tool {
 public:
  AttemptOtpFillingTool(
      TaskId task_id,
      ToolDelegate& tool_delegate,
      tabs::TabHandle tab_handle,
      std::vector<PageTarget> trigger_fields,
      bool for_signin,
      AttemptOtpFillingToolRequest::OtpType predicted_otp_type,
      std::unique_ptr<ActorLoginFlowVerifier> actor_login_flow_verifier);
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
  void OnGmailOtpConfirmationResponse(
      ToolCallback callback,
      std::string otp,
      webui::mojom::GmailOtpConfirmationResultPtr response);
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
  AttemptOtpFillingToolRequest::OtpType predicted_otp_type_;

  bool requires_confirmation_ = false;
  std::unique_ptr<ActorLoginFlowVerifier> actor_login_flow_verifier_;

  base::WeakPtrFactory<AttemptOtpFillingTool> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_H_
