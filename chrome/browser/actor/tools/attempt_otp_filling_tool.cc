// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/actor_login_flow_verifier.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_metrics.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/actor/core/actor_switches.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/shared_types.h"
#include "components/autofill/content/browser/renderer_forms_from_browser_form.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace actor {

namespace {

constexpr base::TimeDelta kGmailOtpOptInCoolOffPeriod = base::Days(90);

const char* PredictedOtpTypeToString(AttemptOtpFillingToolRequest::OtpType type) {
  switch (type) {
    case AttemptOtpFillingToolRequest::OtpType::kUnknown:
      return "Unknown";
    case AttemptOtpFillingToolRequest::OtpType::kSms:
      return "Sms";
    case AttemptOtpFillingToolRequest::OtpType::kEmail:
      return "Email";
    case AttemptOtpFillingToolRequest::OtpType::kAuthenticatorApp:
      return "AuthenticatorApp";
  }
  NOTREACHED();
}

// Returns the `RenderFrameHost` containing the OTP fields.
content::RenderFrameHost* GetOtpFrame(
    tabs::TabHandle tab_handle,
    base::span<const autofill::FieldGlobalId> trigger_field_ids) {
  if (trigger_field_ids.empty()) {
    return nullptr;
  }
  tabs::TabInterface* tab = tab_handle.Get();
  content::WebContents* web_contents = tab ? tab->GetContents() : nullptr;
  if (!web_contents) {
    return nullptr;
  }
  return autofill::FindRenderFrameHostByToken(
      *web_contents, trigger_field_ids.front().frame_token);
}

// Returns the `mojom::ActionResultPtr` for a given `FormFillingContextStatus`.
mojom::ActionResultPtr GetResultFromFormFillingStatus(
    autofill::FormFillingContextStatus status) {
  switch (status) {
    case autofill::FormFillingContextStatus::kSecure:
      return MakeOkResult();
    case autofill::FormFillingContextStatus::kInsecureContext:
      return MakeResult(mojom::ActionResultCode::kOtpInsecureContext);
    case autofill::FormFillingContextStatus::kFormNotFound:
      return MakeResult(mojom::ActionResultCode::kFormFillingFieldNotFound);
    case autofill::FormFillingContextStatus::kTabNotAvailable:
      return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }
}

// Returns the metrics event for a given `FormFillingContextStatus`.
AttemptOtpFillingToolEvent GetMetricsEventFromFormFillingStatus(
    autofill::FormFillingContextStatus status) {
  switch (status) {
    case autofill::FormFillingContextStatus::kSecure:
      NOTREACHED();
    case autofill::FormFillingContextStatus::kInsecureContext:
      return AttemptOtpFillingToolEvent::kFormFillingStatusInsecureContext;
    case autofill::FormFillingContextStatus::kFormNotFound:
      return AttemptOtpFillingToolEvent::kFormFillingStatusFormNotFound;
    case autofill::FormFillingContextStatus::kTabNotAvailable:
      return AttemptOtpFillingToolEvent::kFormFillingStatusTabNotAvailable;
  }
}

}  // namespace

AttemptOtpFillingTool::AttemptOtpFillingTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    tabs::TabHandle tab_handle,
    std::vector<PageTarget> trigger_fields,
    bool for_signin,
    AttemptOtpFillingToolRequest::OtpType predicted_otp_type,
    std::unique_ptr<ActorLoginFlowVerifier> actor_login_flow_verifier)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab_handle),
      trigger_fields_(std::move(trigger_fields)),
      for_signin_(for_signin),
      predicted_otp_type_(predicted_otp_type),
      actor_login_flow_verifier_(std::move(actor_login_flow_verifier)) {
  // Guaranteed by validation in `CreateAttemptOtpFillingRequest` in
  // `actor_proto_conversion.cc`.
  CHECK(!trigger_fields_.empty());
  CHECK(actor_login_flow_verifier_);
}

AttemptOtpFillingTool::~AttemptOtpFillingTool() = default;

void AttemptOtpFillingTool::Validate(ToolCallback callback) {
  RecordAttemptOtpFillingEvent(
      AttemptOtpFillingToolEvent::kStartFillingAttempt);

  PrefService* prefs = tool_delegate().GetProfile().GetPrefs();
  bool gmail_otp_filling_enabled =
      autofill::prefs::IsAutofillGmailOtpFillingEnabled(prefs);
  base::Time dismissal_timestamp =
      autofill::prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp(
          prefs);
  base::TimeDelta time_since_last_dismissal =
      base::Time::Now() - dismissal_timestamp;
  bool within_cool_off_period =
      time_since_last_dismissal < kGmailOtpOptInCoolOffPeriod;

  LogJournalEvent(
      "AttemptOtpFillingTool::Validate",
      JournalDetailsBuilder()
          .Add("trigger_fields_count", trigger_fields_.size())
          .Add("gmail_otp_filling_enabled", gmail_otp_filling_enabled)
          .Add("dismissal_timestamp", dismissal_timestamp)
          .Add("time_since_last_dismissal in days",
               time_since_last_dismissal.InDays())
          .Add("within_cool_off_period", within_cool_off_period)
          .Build());

  if (gmail_otp_filling_enabled) {
    std::move(callback).Run(MakeOkResult());
    return;
  }

  if (within_cool_off_period) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kWithinOptInCoolOffPeriod);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUnableToFill,
                   /*requires_page_stabilization=*/false,
                   "Gmail OTP disabled and within cool-off period for Gmail "
                   "OTP opt-in dialog."));
  } else {
    RecordGmailOtpOptInCardInteraction(GmailOtpOptInCardInteraction::kShowCard);
    tool_delegate().RequestToShowGmailOtpOptInDialog(
        base::BindOnce(&AttemptOtpFillingTool::OnGmailOtpOptInResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void AttemptOtpFillingTool::OnGmailOtpOptInResponse(
    ToolCallback callback,
    webui::mojom::GmailOtpOptInResultPtr response) {
  if (!response || response.is_null()) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kOptInNullResponse);
    RecordGmailOtpOptInCardInteraction(
        GmailOtpOptInCardInteraction::kErrorResponse);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUnableToFill,
                   /*requires_page_stabilization=*/false,
                   "Gmail OTP opt-in dialog response is null"));
    return;
  }

  if (response->is_error_reason()) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kOptInErrorResponse);
    RecordGmailOtpOptInCardInteraction(
        GmailOtpOptInCardInteraction::kErrorResponse);
    LogJournalEvent("AttemptOtpFillingTool::OnGmailOtpOptInResponse",
                    JournalDetailsBuilder()
                        .Add("error_reason", response->get_error_reason())
                        .Build());
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUnableToFill,
                   /*requires_page_stabilization=*/false,
                   "Error in Gmail OTP opt-in dialog response"));
    return;
  }

  bool opt_in_permission_granted = response->get_response()->permission_granted;

  PrefService* prefs = tool_delegate().GetProfile().GetPrefs();
  if (!opt_in_permission_granted) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kOptInPermissionDenied);
    RecordGmailOtpOptInCardInteraction(
        GmailOtpOptInCardInteraction::kPermissionDenied);
    autofill::prefs::SetAutofillGmailOtpFillingActivationDismissalTimestamp(
        prefs, base::Time::Now());
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUserDeclinedOptingIntoFilling,
                   /*requires_page_stabilization=*/false,
                   "User declined Gmail OTP opt-in."));
    return;
  }

  RecordGmailOtpOptInCardInteraction(
      GmailOtpOptInCardInteraction::kPermissionGranted);
  autofill::prefs::SetAutofillGmailOtpFillingEnabled(prefs, true);
  autofill::prefs::ClearAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs);
  std::move(callback).Run(MakeOkResult());
}

mojom::ActionResultPtr AttemptOtpFillingTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  tabs::TabInterface* tab = GetTargetTab().Get();

  LogJournalEvent("AttemptOtpFillingTool::TimeOfUseValidation",
                  JournalDetailsBuilder()
                      .Add("tab", !!tab)
                      .Add("last_observation", !!last_observation)
                      .Add("trigger_fields_count", trigger_fields_.size())
                      .Build());

  if (!tab) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kTabWentAwayBeforeInvocation);
    return MakeResult(mojom::ActionResultCode::kTabWentAway,
                      /*requires_page_stabilization=*/false,
                      "Target tab was destroyed before invocation.");
  }

  if (!last_observation) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kNoLastTabObservation);
    return MakeResult(mojom::ActionResultCode::kOtpNoLastTabObservation,
                      /*requires_page_stabilization=*/false,
                      "Last tab observation is null.");
  }

  trigger_field_ids_.clear();
  trigger_field_ids_.reserve(trigger_fields_.size());
  for (const auto& trigger_field : trigger_fields_) {
    autofill::FieldGlobalId field_id =
        GetFieldIdFromPageTarget(last_observation, tab, trigger_field);
    if (!field_id) {
      RecordAttemptOtpFillingEvent(
          AttemptOtpFillingToolEvent::kTriggerFieldNotFound);
      return MakeResult(mojom::ActionResultCode::kOtpFieldNotFound,
                        /*requires_page_stabilization=*/false,
                        "Trigger field not found.");
    }
    trigger_field_ids_.push_back(field_id);
  }

  autofill::FormFillingContextStatus status =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ValidateFormFillingContext(GetTargetTab(), trigger_field_ids_);
  LogJournalEvent("AttemptOtpFillingTool::Validate",
                  JournalDetailsBuilder()
                      .Add("form filling context status", status)
                      .Build());
  mojom::ActionResultPtr action_result = GetResultFromFormFillingStatus(status);

  if (!IsOk(*action_result)) {
    RecordAttemptOtpFillingEvent(GetMetricsEventFromFormFillingStatus(status));
  }
  return action_result;
}

void AttemptOtpFillingTool::Invoke(ToolCallback callback) {
  journal().Log(
      JournalURL(), task_id(), "AttemptOtpFillingTool::Invoke",
      JournalDetailsBuilder()
          .Add("trigger_fields_count", trigger_field_ids_.size())
          .Add("for_signin", for_signin_)
          .Add("predicted_otp_type",
               PredictedOtpTypeToString(predicted_otp_type_))
          .Build());

  ukm::SourceId source_id = ukm::kInvalidSourceId;
  if (tabs::TabInterface* tab = GetTargetTab().Get()) {
    if (content::WebContents* web_contents = tab->GetContents()) {
      source_id = web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    }
  }
  RecordPredictedOtpTypeMetrics(predicted_otp_type_, source_id);

  content::RenderFrameHost* otp_frame =
      GetOtpFrame(GetTargetTab(), trigger_field_ids_);
  if (!otp_frame) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kNoTargetFrameWithOtpFound);
    LogJournalEvent("AttemptOtpFillingTool::Invoke",
                    JournalDetailsBuilder()
                        .Add("error", "No frame containing an OTP")
                        .Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            MakeResult(mojom::ActionResultCode::kOtpTargetFrameNotFound,
                       /*requires_page_stabilization=*/false,
                       "Target frame containing OTP fields not found.")));
    return;
  }

  // Consume the context. The service clears its state and stops observing.
  std::optional<autofill::ActorLoginContext> context =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ConsumeLoginContext();

  actor_login_flow_verifier_->VerifyIsActorLoginFlow(
      otp_frame->GetFrameTreeNodeId(), otp_frame->GetLastCommittedOrigin(),
      context,
      base::BindOnce(&AttemptOtpFillingTool::OnActorLoginFlowChecked,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnActorLoginFlowChecked(ToolCallback callback,
                                                    bool is_actor_login) {
  bool bypass_login_check =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAttemptOtpFillingBypassLoginCheck);
  LogJournalEvent(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      JournalDetailsBuilder()
          .Add("is_actor_login", is_actor_login)
          .Add("bypass_login_check", bypass_login_check)
          .Build());

  requires_confirmation_ = !is_actor_login && !bypass_login_check;

  if (requires_confirmation_) {
    RecordAttemptOtpFillingEvent(AttemptOtpFillingToolEvent::kNoActorLogin);
    LogJournalEvent(
        "AttemptOtpFillingTool::OnActorLoginFlowChecked",
        JournalDetailsBuilder()
            .Add("status",
                 "Not an Actor Login flow, will require confirmation dialog")
            .Build());
  }

  content::RenderFrameHost* otp_frame =
      GetOtpFrame(GetTargetTab(), trigger_field_ids_);
  if (!otp_frame) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kNoTargetFrameWithOtpFound);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpTargetFrameNotFound,
                   /*requires_page_stabilization=*/false,
                   "Target frame containing OTP fields not found."));
    return;
  }

  tool_delegate().GetActorOneTimeTokenFillingService().RetrieveOtp(
      GetTargetTab(), otp_frame->GetLastCommittedOrigin(), trigger_field_ids_,
      is_actor_login,
      base::BindOnce(&AttemptOtpFillingTool::OnOtpRetrieved,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnOtpRetrieved(
    ToolCallback callback,
    base::expected<std::string, one_time_tokens::OneTimeTokenRetrievalError>
        result) {
  LogJournalEvent(
      "AttemptOtpFillingTool::OnOtpRetrieved",
      JournalDetailsBuilder().Add("otp_received", result.has_value()).Build());

  if (!result.has_value()) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kOtpRetrievalError);
    mojom::ActionResultCode code = mojom::ActionResultCode::kOtpRetrievalError;
    std::string message =
        base::StringPrintf("An error occurred during OTP retrieval: %d",
                           std::to_underlying(result.error()));

    LogJournalEvent("AttemptOtpFillingTool::OnOtpRetrieved",
                    JournalDetailsBuilder()
                        .Add("error", message)
                        .Add("error_code", result.error())
                        .Build());

    using enum one_time_tokens::OneTimeTokenRetrievalError;
    switch (result.error()) {
      case kGmailOtpBackendSmartFeaturesInGmailConsentRequired:
        code = mojom::ActionResultCode::kOtpGmailConsentRequired;
        message = "Gmail Smart Features consent is required.";
        break;
      case kGmailOtpBackendSmartFeaturesInOtherGoogleProductsConsentRequired:
        code = mojom::ActionResultCode::kOtpGoogleConsentRequired;
        message = "Google Smart Features consent is required.";
        break;
      case kGmailOtpBackendDmaCrossProductSharingConsentRequired:
        code = mojom::ActionResultCode::kOtpDmaConsentRequired;
        message = "DMA cross-product sharing consent is required.";
        break;
      case kGmailOtpBackendOtpAttributeNotFound:
        code = mojom::ActionResultCode::kOtpNoCodeFound;
        message = "Failed to extract verification code from the OTP email.";
        break;
      case kGmailOtpBackendOneTimeTokenExpired:
        code = mojom::ActionResultCode::kOtpExpired;
        message = "The retrieved OTP has expired.";
        break;
      case kGmailOtpBackendApiNotAvailable:
      case kGmailOtpBackendInitializationFailed:
        code = mojom::ActionResultCode::kOtpServiceUnavailable;
        message = "OTP filling service is not available.";
        break;
      case kSubscriptionExpired:
        code = mojom::ActionResultCode::kOtpRetrievalTimeout;
        message = "OTP retrieval timed out.";
        break;
      default:
        break;
    }
    std::move(callback).Run(
        MakeResult(code, /*requires_page_stabilization=*/false, message));
    return;
  }

  std::string retrieved_otp = result.value();

  mojom::ActionResultPtr validation_result = GetResultFromFormFillingStatus(
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ValidateFormFillingContext(GetTargetTab(), trigger_field_ids_));
  if (!IsOk(*validation_result)) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kFormFillingNotSecureBeforeFilling);
    std::move(callback).Run(std::move(validation_result));
    return;
  }

  if (requires_confirmation_) {
    LogJournalEvent("AttemptOtpFillingTool::OnOtpRetrieved",
                    JournalDetailsBuilder()
                        .Add("status", "Showing Gmail OTP confirmation dialog")
                        .Build());
    std::string otp_code = retrieved_otp;
    tool_delegate().RequestToShowGmailOtpConfirmationDialog(
        otp_code,
        base::BindOnce(&AttemptOtpFillingTool::OnGmailOtpConfirmationResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(retrieved_otp)));
    return;
  }

  tool_delegate().GetActorOneTimeTokenFillingService().FillOtp(
      GetTargetTab(), trigger_field_ids_, std::move(retrieved_otp),
      base::BindOnce(&AttemptOtpFillingTool::OnOtpFilled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnOtpFilled(ToolCallback callback, bool success) {
  LogJournalEvent("AttemptOtpFillingTool::OnOtpFilled",
                  JournalDetailsBuilder().Add("success", success).Build());

  if (success) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kFillingOtpSuccess);
    std::move(callback).Run(MakeOkResult());
  } else {
    RecordAttemptOtpFillingEvent(AttemptOtpFillingToolEvent::kFillingOtpError);
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kOtpFillFailure,
                                       /*requires_page_stabilization=*/false,
                                       "Failed to fill OTP."));
  }
}

void AttemptOtpFillingTool::LogJournalEvent(
    std::string_view event_name,
    std::vector<mojom::JournalDetailsPtr> journal_details) {
  journal().Log(JournalURL(), task_id(), event_name,
                std::move(journal_details));
}

void AttemptOtpFillingTool::UpdateTaskBeforeInvoke(
    ActorTask& task,
    ToolCallback callback) const {
  task.AddTab(GetTargetTab(), /*stop_task_on_detach=*/true,
              std::move(callback));
}

std::string AttemptOtpFillingTool::DebugString() const {
  // This ends up in chrome://actor-internals and will be used for debugging.
  return "AttemptOtpFillingTool";
}

std::string AttemptOtpFillingTool::JournalEvent() const {
  return "AttemptOtpFillingTool";
}

std::unique_ptr<ObservationDelayController>
AttemptOtpFillingTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  tabs::TabInterface* tab = GetTargetTab().Get();
  if (!tab || !tab->GetContents()) {
    return nullptr;
  }

  content::RenderFrameHost* rfh = tab->GetContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return nullptr;
  }

  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

tabs::TabHandle AttemptOtpFillingTool::GetTargetTab() const {
  return tab_handle_;
}

void AttemptOtpFillingTool::OnGmailOtpConfirmationResponse(
    ToolCallback callback,
    std::string otp,
    webui::mojom::GmailOtpConfirmationResultPtr response) {
  if (!response || response.is_null()) {
    LogJournalEvent("AttemptOtpFillingTool::OnGmailOtpConfirmationResponse",
                    JournalDetailsBuilder()
                        .Add("error", "Gmail OTP confirmation response is null")
                        .Build());
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUnableToFill,
                   /*requires_page_stabilization=*/false,
                   "Gmail OTP confirmation response is null"));
    return;
  }

  if (response->is_error_reason()) {
    LogJournalEvent("AttemptOtpFillingTool::OnGmailOtpConfirmationResponse",
                    JournalDetailsBuilder()
                        .Add("error_reason", response->get_error_reason())
                        .Build());
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUnableToFill,
                   /*requires_page_stabilization=*/false,
                   "Error in Gmail OTP confirmation response"));
    return;
  }

  if (!response->is_response()) {
    LogJournalEvent(
        "AttemptOtpFillingTool::OnGmailOtpConfirmationResponse",
        JournalDetailsBuilder()
            .Add("error",
                 "Gmail OTP confirmation response lacks response payload")
            .Build());
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUnableToFill,
                   /*requires_page_stabilization=*/false,
                   "Gmail OTP confirmation response is invalid"));
    return;
  }
  bool permission_granted = response->get_response()->permission_granted;
  LogJournalEvent("AttemptOtpFillingTool::OnGmailOtpConfirmationResponse",
                  JournalDetailsBuilder()
                      .Add("permission_granted", permission_granted)
                      .Build());

  if (!permission_granted) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUserDeclinedOptingIntoFilling,
                   /*requires_page_stabilization=*/false,
                   "User declined Gmail OTP confirmation."));
    return;
  }

  mojom::ActionResultPtr validation_result = GetResultFromFormFillingStatus(
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ValidateFormFillingContext(GetTargetTab(), trigger_field_ids_));
  if (!IsOk(*validation_result)) {
    RecordAttemptOtpFillingEvent(
        AttemptOtpFillingToolEvent::kFormFillingNotSecureBeforeFilling);
    std::move(callback).Run(std::move(validation_result));
    return;
  }

  tool_delegate().GetActorOneTimeTokenFillingService().FillOtp(
      GetTargetTab(), trigger_field_ids_, std::move(otp),
      base::BindOnce(&AttemptOtpFillingTool::OnOtpFilled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

}  // namespace actor
