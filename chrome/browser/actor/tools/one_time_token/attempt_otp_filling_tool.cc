// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_tool.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/one_time_token/actor_login_flow_verifier.h"
#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_metrics.h"
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
#include "components/one_time_tokens/core/browser/user_data_processing_consent_states.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace actor {

namespace {

// Describes the status of the target form filling context during validation.
enum class FormFillingContextStatus {
  // The tab, form, and security state are valid and cryptographic HTTPS is
  // verified.
  kSecure,
  // The page or form is insecure (e.g., mixed content or HTTP submission).
  kInsecureContext,
  // The form structure or Autofill manager could not be found.
  kFormNotFound,
  // The target tab is null or has no `WebContents`.
  kTabNotAvailable,
};

// Maps the detailed validation result from the service to the coarser status
// used for the tool's action results and `ToolInvocation` metrics.
FormFillingContextStatus ToFormFillingContextStatus(
    autofill::FormFillingContextValidationResult result) {
  using enum autofill::FormFillingContextValidationResult;
  switch (result) {
    case kSecure:
      return FormFillingContextStatus::kSecure;
    case kTabNotAvailable:
      return FormFillingContextStatus::kTabNotAvailable;
    case kInsecurePage:
    case kMixedContentFormAction:
    case kOtpFieldsFromDifferentOrigins:
    case kInvalidFrameOrOrigin:
      return FormFillingContextStatus::kInsecureContext;
    case kEmptyTriggerFieldIds:
    case kNoAutofillManager:
    case kFormStructureNotFound:
    case kTriggerFieldNotFound:
    case kFrameNotFound:
      return FormFillingContextStatus::kFormNotFound;
  }
  NOTREACHED();
}

constexpr base::TimeDelta kGmailOtpOptInCoolOffPeriod = base::Days(90);

std::string GetMockOtpValue() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAttemptOtpFillingToolMockValueSkipsChecks);
}

const char* PredictedOtpTypeToString(
    AttemptOtpFillingToolRequest::OtpType type) {
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

autofill::ActorOneTimeTokenFillingService::PredictedOtpType
ToServicePredictedOtpType(AttemptOtpFillingToolRequest::OtpType type) {
  using enum autofill::ActorOneTimeTokenFillingService::PredictedOtpType;
  switch (type) {
    case AttemptOtpFillingToolRequest::OtpType::kUnknown:
      return kUnknown;
    case AttemptOtpFillingToolRequest::OtpType::kSms:
      return kSms;
    case AttemptOtpFillingToolRequest::OtpType::kEmail:
      return kEmail;
    case AttemptOtpFillingToolRequest::OtpType::kAuthenticatorApp:
      return kAuthenticatorApp;
  }
  NOTREACHED();
}

const char* ConsentStateToString(one_time_tokens::ConsentState state) {
  switch (state) {
    case one_time_tokens::ConsentState::kUndefined:
      return "Undefined";
    case one_time_tokens::ConsentState::kUnknown:
      return "Unknown";
    case one_time_tokens::ConsentState::kEnabled:
      return "Enabled";
    case one_time_tokens::ConsentState::kDisabled:
      return "Disabled";
  }
  NOTREACHED();
}

// Returns the `RenderFrameHost` containing the OTP fields.
content::RenderFrameHost* GetOtpFrame(
    ActorSurfaceHandle actor_surface_handle,
    base::span<const autofill::FieldGlobalId> trigger_field_ids) {
  if (trigger_field_ids.empty()) {
    return nullptr;
  }
  ActorSurface* actor_surface = actor_surface_handle.Get();
  content::WebContents* web_contents =
      actor_surface ? actor_surface->GetWebContents() : nullptr;
  if (!web_contents) {
    return nullptr;
  }
  return autofill::FindRenderFrameHostByToken(
      *web_contents, trigger_field_ids.front().frame_token);
}

content::WebContents* GetWebContentsForOtpFilling(
    ActorSurfaceHandle actor_surface_handle) {
  ActorSurface* actor_surface = actor_surface_handle.Get();
  return actor_surface ? actor_surface->GetWebContents() : nullptr;
}

// Returns the `mojom::ActionResultPtr` for a given `FormFillingContextStatus`.
mojom::ActionResultPtr GetResultFromFormFillingStatus(
    FormFillingContextStatus status) {
  switch (status) {
    case FormFillingContextStatus::kSecure:
      return MakeOkResult();
    case FormFillingContextStatus::kInsecureContext:
      return MakeResult(mojom::ActionResultCode::kOtpInsecureContext);
    case FormFillingContextStatus::kFormNotFound:
      return MakeResult(mojom::ActionResultCode::kFormFillingFieldNotFound);
    case FormFillingContextStatus::kTabNotAvailable:
      return MakeResult(mojom::ActionResultCode::kTabWentAway);
  }
}

// Returns the metrics event for a `FormFillingContextStatus` that failed the
// time-of-use check in `TimeOfUseValidation()`, before the OTP is retrieved.
// `GetBeforeFillingMetricsEvent()` covers the later checks right before
// filling.
AttemptOtpFillingToolEvent GetMetricsEventFromFormFillingStatus(
    FormFillingContextStatus status) {
  switch (status) {
    case FormFillingContextStatus::kSecure:
      NOTREACHED();
    case FormFillingContextStatus::kInsecureContext:
      return AttemptOtpFillingToolEvent::kFormFillingStatusInsecureContext;
    case FormFillingContextStatus::kFormNotFound:
      return AttemptOtpFillingToolEvent::kFormFillingStatusFormNotFound;
    case FormFillingContextStatus::kTabNotAvailable:
      return AttemptOtpFillingToolEvent::kFormFillingStatusTabNotAvailable;
  }
}

// Returns the metrics event for a `FormFillingContextStatus` that failed the
// check right before filling the OTP, i.e. after the OTP is retrieved
// (`OnOtpRetrieved()`) or after the user confirms it
// (`OnGmailOtpConfirmationResponse()`). Uses the *BeforeFilling buckets so
// these failures are distinguishable from the time-of-use failures recorded
// via `GetMetricsEventFromFormFillingStatus()`.
AttemptOtpFillingToolEvent GetBeforeFillingMetricsEvent(
    FormFillingContextStatus status) {
  switch (status) {
    case FormFillingContextStatus::kSecure:
      // Guaranteed by the caller's IsOk() check.
      NOTREACHED();
    case FormFillingContextStatus::kInsecureContext:
      return AttemptOtpFillingToolEvent::kFormFillingNotSecureBeforeFilling;
    case FormFillingContextStatus::kFormNotFound:
      return AttemptOtpFillingToolEvent::kFormNotFoundBeforeFilling;
    case FormFillingContextStatus::kTabNotAvailable:
      return AttemptOtpFillingToolEvent::kTabNotAvailableBeforeFilling;
  }
}

// Returns the row or column of `state` in the lookup table of
// `GetGmailAndGoogleSmartFeaturesConsent()`.
size_t GetConsentStateIndex(one_time_tokens::ConsentState state) {
  switch (state) {
    case one_time_tokens::ConsentState::kEnabled:
      return 0;
    case one_time_tokens::ConsentState::kDisabled:
      return 1;
    case one_time_tokens::ConsentState::kUnknown:
      return 2;
    case one_time_tokens::ConsentState::kUndefined:
      return 3;
  }
  NOTREACHED();
}

// Returns the metrics bucket for the combination of `consent_states`.
ActorOtpGmailAndGoogleSmartFeaturesConsent
GetGmailAndGoogleSmartFeaturesConsent(
    const one_time_tokens::UserDataProcessingConsentStates& consent_states) {
  using enum ActorOtpGmailAndGoogleSmartFeaturesConsent;
  // Rows: Gmail (`comms_apps`) state. Columns: Google (`google_apps`) state.
  // Both in `GetConsentStateIndex()` order: Enabled, Disabled, Unknown,
  // Undefined.
  static constexpr std::array<
      std::array<ActorOtpGmailAndGoogleSmartFeaturesConsent, 4>, 4>
      kTable = {{
          {kGmailEnabledGoogleEnabled, kGmailEnabledGoogleDisabled,
           kGmailEnabledGoogleUnknown, kGmailEnabledGoogleUndefined},
          {kGmailDisabledGoogleEnabled, kGmailDisabledGoogleDisabled,
           kGmailDisabledGoogleUnknown, kGmailDisabledGoogleUndefined},
          {kGmailUnknownGoogleEnabled, kGmailUnknownGoogleDisabled,
           kGmailUnknownGoogleUnknown, kGmailUnknownGoogleUndefined},
          {kGmailUndefinedGoogleEnabled, kGmailUndefinedGoogleDisabled,
           kGmailUndefinedGoogleUnknown, kGmailUndefinedGoogleUndefined},
      }};
  return kTable[GetConsentStateIndex(consent_states.comms_apps)]
               [GetConsentStateIndex(consent_states.google_apps)];
}
}  // namespace

AttemptOtpFillingTool::AttemptOtpFillingTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    ActorSurface& actor_surface,
    std::vector<PageTarget> trigger_fields,
    bool for_signin,
    AttemptOtpFillingToolRequest::OtpType predicted_otp_type,
    std::unique_ptr<ActorLoginFlowVerifier> actor_login_flow_verifier)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(actor_surface.GetHandle()),
      trigger_fields_(std::move(trigger_fields)),
      for_signin_(for_signin),
      predicted_otp_type_(predicted_otp_type),
      actor_login_flow_verifier_(std::move(actor_login_flow_verifier)) {
  // Guaranteed by validation in `CreateAttemptOtpFillingRequest` in
  // `actor_proto_conversion.cc`.
  CHECK(!trigger_fields_.empty());
  CHECK(actor_login_flow_verifier_);
}

AttemptOtpFillingTool::~AttemptOtpFillingTool() {
  if (!pending_cancellation_event_) {
    return;
  }
  RecordEvent(*pending_cancellation_event_);
  if (*pending_cancellation_event_ ==
      AttemptOtpFillingToolEvent::kCancelledDuringOptInCard) {
    RecordGmailOtpOptInCardInteraction(
        GmailOtpOptInCardInteraction::kCancelled);
  } else if (*pending_cancellation_event_ ==
             AttemptOtpFillingToolEvent::kCancelledDuringConfirmationDialog) {
    RecordGmailOtpConfirmationDialogInteraction(
        GmailOtpConfirmationDialogInteraction::kCancelled);
  }
}

void AttemptOtpFillingTool::Validate(ToolCallback callback) {
  if (!GetMockOtpValue().empty()) {
    LogJournalEvent("AttemptOtpFillingTool::Validate",
                    JournalDetailsBuilder()
                        .Add("status", "Using mock OTP, skipping validation")
                        .Build());
    std::move(callback).Run(MakeOkResult());
    return;
  }

  PrefService* prefs = tool_delegate().GetProfile().GetPrefs();
  bool gmail_otp_filling_enabled =
      autofill::prefs::IsAutofillGmailOtpFillingEnabled(prefs);
  opt_in_state_ = gmail_otp_filling_enabled
                      ? AttemptOtpFillingOptInState::kAlreadyOptedIn
                      : AttemptOtpFillingOptInState::kNotOptedIn;
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

  pending_cancellation_event_ =
      AttemptOtpFillingToolEvent::kCancelledDuringConsentFetch;
  tool_delegate()
      .GetActorOneTimeTokenFillingService()
      .FetchUserDataProcessingConsent(base::BindOnce(
          &AttemptOtpFillingTool::OnUserDataProcessingConsentFetched,
          weak_factory_.GetWeakPtr(), std::move(callback),
          base::TimeTicks::Now(), gmail_otp_filling_enabled,
          within_cool_off_period));
}

void AttemptOtpFillingTool::OnUserDataProcessingConsentFetched(
    ToolCallback callback,
    base::TimeTicks start_time,
    bool gmail_otp_filling_enabled,
    bool within_cool_off_period,
    std::optional<one_time_tokens::UserDataProcessingConsentStates>
        consent_states) {
  // The awaited step has finished; see `pending_cancellation_event_` for more
  // details.
  pending_cancellation_event_.reset();
  RecordConsentFetchDuration(base::TimeTicks::Now() - start_time,
                             consent_states.has_value());
  if (!consent_states.has_value()) {
    RecordEvent(AttemptOtpFillingToolEvent::
                    kUnableToRetrieveGmailAndGoogleSmartFeaturesConsent);
    LogJournalEvent("AttemptOtpFillingTool::OnUserDataProcessingConsentFetched",
                    JournalDetailsBuilder()
                        .Add("error", "Consent states fetch returned nullopt")
                        .Build());

    std::move(callback).Run(MakeResult(
        mojom::ActionResultCode::kOtpUnableToFill,
        /*requires_page_stabilization=*/false,
        "Could not retrieve Gmail and Google Smart Features consent."));
    return;
  }

  LogJournalEvent("AttemptOtpFillingTool::OnUserDataProcessingConsentFetched",
                  JournalDetailsBuilder()
                      .Add("In-product User Data Processing (iUDP, comms_apps)",
                           ConsentStateToString(consent_states->comms_apps))
                      .Add("Google User Data Processing (gUDP, google_apps)",
                           ConsentStateToString(consent_states->google_apps))
                      .Build());

  RecordGmailAndGoogleSmartFeaturesConsent(
      GetGmailAndGoogleSmartFeaturesConsent(*consent_states));

  if (consent_states->comms_apps == one_time_tokens::ConsentState::kDisabled) {
    RecordEvent(AttemptOtpFillingToolEvent::kGmailSmartFeaturesConsentRequired);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpGmailConsentRequired,
                   /*requires_page_stabilization=*/false,
                   "Gmail Smart Features consent is required."));
    return;
  }

  if (consent_states->google_apps == one_time_tokens::ConsentState::kDisabled) {
    RecordEvent(
        AttemptOtpFillingToolEvent::kGoogleSmartFeaturesConsentRequired);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpGoogleConsentRequired,
                   /*requires_page_stabilization=*/false,
                   "Google Smart Features consent is required."));
    return;
  }

  if (gmail_otp_filling_enabled) {
    std::move(callback).Run(MakeOkResult());
    return;
  }

  if (within_cool_off_period) {
    RecordEvent(AttemptOtpFillingToolEvent::kWithinOptInCoolOffPeriod);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUnableToFill,
                   /*requires_page_stabilization=*/false,
                   "Gmail OTP disabled and within cool-off period for Gmail "
                   "OTP opt-in dialog."));
    return;
  }

  pending_cancellation_event_ =
      AttemptOtpFillingToolEvent::kCancelledDuringOptInCard;
  tool_delegate().RequestToShowGmailOtpOptInDialog(
      base::BindOnce(&AttemptOtpFillingTool::OnGmailOtpOptInResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnGmailOtpOptInResponse(
    ToolCallback callback,
    webui::mojom::GmailOtpOptInResultPtr response) {
  // The awaited step has finished; see `pending_cancellation_event_` for more
  // details.
  pending_cancellation_event_.reset();
  if (!response || response.is_null()) {
    RecordEvent(AttemptOtpFillingToolEvent::kOptInNullResponse);
    RecordGmailOtpOptInCardInteraction(
        GmailOtpOptInCardInteraction::kErrorResponse);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUnableToFill,
                   /*requires_page_stabilization=*/false,
                   "Gmail OTP opt-in dialog response is null"));
    return;
  }

  if (response->is_error_reason()) {
    RecordEvent(AttemptOtpFillingToolEvent::kOptInErrorResponse);
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
    RecordEvent(AttemptOtpFillingToolEvent::kOptInPermissionDenied);
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
  opt_in_state_ = AttemptOtpFillingOptInState::kNewlyOptedIn;
  autofill::prefs::ClearAutofillGmailOtpFillingActivationDismissalTimestamp(
      prefs);
  std::move(callback).Run(MakeOkResult());
}

mojom::ActionResultPtr AttemptOtpFillingTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  ActorSurface* actor_surface = actor_surface_handle_.Get();

  LogJournalEvent("AttemptOtpFillingTool::TimeOfUseValidation",
                  JournalDetailsBuilder()
                      .Add("actor_surface", !!actor_surface)
                      .Add("last_observation", !!last_observation)
                      .Add("trigger_fields_count", trigger_fields_.size())
                      .Build());

  if (!actor_surface) {
    RecordEvent(AttemptOtpFillingToolEvent::kTabWentAwayBeforeInvocation);
    return MakeResult(mojom::ActionResultCode::kTabWentAway,
                      /*requires_page_stabilization=*/false,
                      "Target tab was destroyed before invocation.");
  }

  if (!last_observation) {
    RecordEvent(AttemptOtpFillingToolEvent::kNoLastTabObservation);
    return MakeResult(mojom::ActionResultCode::kOtpNoLastTabObservation,
                      /*requires_page_stabilization=*/false,
                      "Last tab observation is null.");
  }

  trigger_field_ids_.clear();
  trigger_field_ids_.reserve(trigger_fields_.size());
  for (const auto& trigger_field : trigger_fields_) {
    autofill::FieldGlobalId field_id = GetFieldIdFromPageTarget(
        last_observation, actor_surface_handle_, trigger_field);
    if (!field_id) {
      RecordEvent(AttemptOtpFillingToolEvent::kTriggerFieldNotFound);
      return MakeResult(mojom::ActionResultCode::kOtpFieldNotFound,
                        /*requires_page_stabilization=*/false,
                        "Trigger field not found.");
    }
    trigger_field_ids_.push_back(field_id);
  }

  const autofill::FormFillingContextValidationResult context_result =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ValidateFormFillingContext(
              GetWebContentsForOtpFilling(actor_surface_handle_),
              trigger_field_ids_);
  RecordFormFillingContextValidationResult(AttemptOtpFillingStep::kTimeOfUse,
                                           context_result);
  const FormFillingContextStatus status =
      ToFormFillingContextStatus(context_result);
  LogJournalEvent("AttemptOtpFillingTool::Validate",
                  JournalDetailsBuilder()
                      .Add("form filling context status", status)
                      .Build());
  mojom::ActionResultPtr action_result = GetResultFromFormFillingStatus(status);

  if (!IsOk(*action_result)) {
    RecordEvent(GetMetricsEventFromFormFillingStatus(status));
  }
  return action_result;
}

void AttemptOtpFillingTool::Invoke(ToolCallback callback) {
  journal().Log(JournalURL(), task_id(), "AttemptOtpFillingTool::Invoke",
                JournalDetailsBuilder()
                    .Add("trigger_fields_count", trigger_field_ids_.size())
                    .Add("for_signin", for_signin_)
                    .Add("predicted_otp_type",
                         PredictedOtpTypeToString(predicted_otp_type_))
                    .Build());

  ukm::SourceId source_id = ukm::kInvalidSourceId;
  if (ActorSurface* actor_surface = actor_surface_handle_.Get()) {
    if (content::WebContents* web_contents = actor_surface->GetWebContents()) {
      source_id = web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
    }
  }
  RecordPredictedOtpTypeMetrics(predicted_otp_type_, source_id);

  content::RenderFrameHost* otp_frame =
      GetOtpFrame(GetTargetActorSurface(), trigger_field_ids_);
  if (!otp_frame) {
    RecordEvent(AttemptOtpFillingToolEvent::kNoTargetFrameWithOtpFound);
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

  std::string mock_otp = GetMockOtpValue();
  if (!mock_otp.empty()) {
    tool_delegate().GetActorOneTimeTokenFillingService().ConsumeLoginContext();
    LogJournalEvent(
        "AttemptOtpFillingTool::Invoke",
        JournalDetailsBuilder().Add("status", "Using mock OTP").Build());
    // The mock path records ToolInvocation events like a real run.
    pending_cancellation_event_ =
        AttemptOtpFillingToolEvent::kCancelledDuringFillOtp;
    tool_delegate().GetActorOneTimeTokenFillingService().FillOtp(
        GetWebContentsForOtpFilling(actor_surface_handle_), trigger_field_ids_,
        std::move(mock_otp),
        base::BindOnce(&AttemptOtpFillingTool::OnOtpFilled,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::optional<url::Origin> context_origin =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .GetLoginContextOrigin();
  bool should_use_strong_matching =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .GetLoginContextShouldUseStrongMatching();

  ActorSurface* actor_surface = actor_surface_handle_.Get();
  content::WebContents* web_contents =
      actor_surface ? actor_surface->GetWebContents() : nullptr;
  url::Origin main_frame_origin =
      web_contents
          ? web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin()
          : url::Origin();

  LogJournalEvent(
      "AttemptOtpFillingTool::Invoke",
      JournalDetailsBuilder()
          .Add("otp_frame_id", otp_frame->GetFrameTreeNodeId())
          .Add("otp_frame_origin", otp_frame->GetLastCommittedOrigin())
          .Add("main_frame_origin", main_frame_origin)
          .Add("should_use_strong_matching", should_use_strong_matching)
          .Build());

  pending_cancellation_event_ =
      AttemptOtpFillingToolEvent::kCancelledDuringLoginFlowVerification;
  actor_login_flow_verifier_->VerifyIsActorLoginFlow(
      otp_frame->GetFrameTreeNodeId(), otp_frame->GetLastCommittedOrigin(),
      main_frame_origin, context_origin, should_use_strong_matching,
      base::BindOnce(
          [](base::WeakPtr<autofill::ActorOneTimeTokenFillingService> service)
              -> std::optional<autofill::ActorLoginContext> {
            return service ? service->ConsumeLoginContext() : std::nullopt;
          },
          tool_delegate().GetActorOneTimeTokenFillingService().GetWeakPtr()),
      base::BindOnce(&AttemptOtpFillingTool::OnActorLoginFlowChecked,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnActorLoginFlowChecked(
    ToolCallback callback,
    ActorLoginFlowVerifier::Result result) {
  // The awaited step has finished; see `pending_cancellation_event_` for more
  // details.
  pending_cancellation_event_.reset();
  RecordActorLoginFlowVerification(result);

  bool is_actor_login = ActorLoginFlowVerifier::IsSuccess(result);
  LogJournalEvent("AttemptOtpFillingTool::OnActorLoginFlowChecked",
                  JournalDetailsBuilder()
                      .Add("result", result)
                      .Add("is_actor_login", is_actor_login)
                      .Build());

  requires_confirmation_ = !is_actor_login;

  if (requires_confirmation_) {
    LogJournalEvent(
        "AttemptOtpFillingTool::OnActorLoginFlowChecked",
        JournalDetailsBuilder()
            .Add("status",
                 "Not an Actor Login flow, will require confirmation dialog")
            .Build());
  }

  content::RenderFrameHost* otp_frame =
      GetOtpFrame(GetTargetActorSurface(), trigger_field_ids_);
  if (!otp_frame) {
    RecordEvent(AttemptOtpFillingToolEvent::kNoTargetFrameWithOtpFound);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpTargetFrameNotFound,
                   /*requires_page_stabilization=*/false,
                   "Target frame containing OTP fields not found."));
    return;
  }

  LogJournalEvent(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      JournalDetailsBuilder().Add("status", "Calling RetrieveOtp").Build());
  pending_cancellation_event_ =
      AttemptOtpFillingToolEvent::kCancelledDuringOtpRetrieval;
  tool_delegate().GetActorOneTimeTokenFillingService().RetrieveOtp(
      GetWebContentsForOtpFilling(actor_surface_handle_),
      otp_frame->GetLastCommittedOrigin(), trigger_field_ids_, is_actor_login,
      ToServicePredictedOtpType(predicted_otp_type_),
      base::BindOnce(&AttemptOtpFillingTool::OnOtpRetrieved,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnOtpRetrieved(
    ToolCallback callback,
    base::expected<std::string, one_time_tokens::OneTimeTokenRetrievalError>
        result) {
  // The awaited step has finished; see `pending_cancellation_event_` for more
  // details.
  pending_cancellation_event_.reset();
  LogJournalEvent(
      "AttemptOtpFillingTool::OnOtpRetrieved",
      JournalDetailsBuilder().Add("otp_received", result.has_value()).Build());

  if (!result.has_value()) {
    AttemptOtpFillingToolEvent event =
        AttemptOtpFillingToolEvent::kOtpRetrievalError;
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
      // A sender-domain mismatch is still a timeout for the model: no OTP from
      // an allowed sender arrived in time. Only the metric tells them apart.
      case kGmailOtpSenderDomainMismatch:
        event = AttemptOtpFillingToolEvent::kGmailOtpSenderDomainMismatch;
        code = mojom::ActionResultCode::kOtpRetrievalTimeout;
        message = "OTP retrieval timed out.";
        break;
      case kSubscriptionExpired:
        event = AttemptOtpFillingToolEvent::kOtpRetrievalTimeout;
        code = mojom::ActionResultCode::kOtpRetrievalTimeout;
        message = "OTP retrieval timed out.";
        break;
      default:
        break;
    }
    RecordEvent(event);
    std::move(callback).Run(
        MakeResult(code, /*requires_page_stabilization=*/false, message));
    return;
  }

  std::string retrieved_otp = result.value();

  const autofill::FormFillingContextValidationResult context_result =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ValidateFormFillingContext(
              GetWebContentsForOtpFilling(actor_surface_handle_),
              trigger_field_ids_);
  RecordFormFillingContextValidationResult(
      AttemptOtpFillingStep::kAfterOtpRetrieved, context_result);
  const FormFillingContextStatus status =
      ToFormFillingContextStatus(context_result);
  mojom::ActionResultPtr validation_result =
      GetResultFromFormFillingStatus(status);
  if (!IsOk(*validation_result)) {
    LogJournalEvent("AttemptOtpFillingTool::OnOtpRetrieved",
                    JournalDetailsBuilder()
                        .AddError("Form filling context validation failed")
                        .Build());
    RecordEvent(GetBeforeFillingMetricsEvent(status));
    std::move(callback).Run(std::move(validation_result));
    return;
  }

  if (requires_confirmation_) {
    LogJournalEvent(
        "AttemptOtpFillingTool::OnOtpRetrieved",
        JournalDetailsBuilder()
            .Add("status", "Requesting to show the confirmation dialog")
            .Build());
    std::string otp_code = retrieved_otp;
    pending_cancellation_event_ =
        AttemptOtpFillingToolEvent::kCancelledDuringConfirmationDialog;
    tool_delegate().RequestToShowGmailOtpConfirmationDialog(
        otp_code,
        base::BindOnce(&AttemptOtpFillingTool::OnGmailOtpConfirmationResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(retrieved_otp)));
    return;
  }

  LogJournalEvent(
      "AttemptOtpFillingTool::OnOtpRetrieved",
      JournalDetailsBuilder()
          .Add("status",
               "Calling FillOtp without showing the confirmation dialog")
          .Build());
  pending_cancellation_event_ =
      AttemptOtpFillingToolEvent::kCancelledDuringFillOtp;
  tool_delegate().GetActorOneTimeTokenFillingService().FillOtp(
      GetWebContentsForOtpFilling(actor_surface_handle_), trigger_field_ids_,
      std::move(retrieved_otp),
      base::BindOnce(&AttemptOtpFillingTool::OnOtpFilled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnOtpFilled(ToolCallback callback, bool success) {
  // The awaited step has finished; see `pending_cancellation_event_` for more
  // details.
  pending_cancellation_event_.reset();
  LogJournalEvent("AttemptOtpFillingTool::OnOtpFilled",
                  JournalDetailsBuilder().Add("success", success).Build());

  if (success) {
    RecordEvent(AttemptOtpFillingToolEvent::kFillingOtpSuccess);
    std::move(callback).Run(MakeOkResult());
  } else {
    RecordEvent(AttemptOtpFillingToolEvent::kFillingOtpError);
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
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface || !actor_surface->GetWebContents()) {
    return nullptr;
  }

  content::RenderFrameHost* rfh =
      actor_surface->GetWebContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return nullptr;
  }

  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

ActorSurfaceHandle AttemptOtpFillingTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

void AttemptOtpFillingTool::OnGmailOtpConfirmationResponse(
    ToolCallback callback,
    std::string otp,
    webui::mojom::GmailOtpConfirmationResultPtr response) {
  // The awaited step has finished; see `pending_cancellation_event_` for more
  // details.
  pending_cancellation_event_.reset();
  if (!response || response.is_null()) {
    LogJournalEvent("AttemptOtpFillingTool::OnGmailOtpConfirmationResponse",
                    JournalDetailsBuilder()
                        .Add("error", "Gmail OTP confirmation response is null")
                        .Build());
    RecordEvent(
        AttemptOtpFillingToolEvent::kGmailOtpConfirmationResponseNotValid);
    RecordGmailOtpConfirmationDialogInteraction(
        GmailOtpConfirmationDialogInteraction::kErrorResponse);
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
    RecordEvent(
        AttemptOtpFillingToolEvent::kGmailOtpConfirmationResponseNotValid);
    RecordGmailOtpConfirmationDialogInteraction(
        GmailOtpConfirmationDialogInteraction::kErrorResponse);
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
    RecordEvent(
        AttemptOtpFillingToolEvent::kGmailOtpConfirmationResponseNotValid);
    RecordGmailOtpConfirmationDialogInteraction(
        GmailOtpConfirmationDialogInteraction::kErrorResponse);
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
    RecordEvent(
        AttemptOtpFillingToolEvent::kGmailOtpConfirmationDeclinedByUser);
    RecordGmailOtpConfirmationDialogInteraction(
        GmailOtpConfirmationDialogInteraction::kPermissionDenied);
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpUserDeclinedFillingConfirmation,
                   /*requires_page_stabilization=*/false,
                   "User declined Gmail OTP confirmation."));
    return;
  }

  RecordGmailOtpConfirmationDialogInteraction(
      GmailOtpConfirmationDialogInteraction::kPermissionGranted);

  const autofill::FormFillingContextValidationResult context_result =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ValidateFormFillingContext(
              GetWebContentsForOtpFilling(actor_surface_handle_),
              trigger_field_ids_);
  RecordFormFillingContextValidationResult(
      AttemptOtpFillingStep::kAfterConfirmation, context_result);
  const FormFillingContextStatus status =
      ToFormFillingContextStatus(context_result);
  mojom::ActionResultPtr validation_result =
      GetResultFromFormFillingStatus(status);
  if (!IsOk(*validation_result)) {
    LogJournalEvent("AttemptOtpFillingTool::OnGmailOtpConfirmationResponse",
                    JournalDetailsBuilder()
                        .AddError("Form filling context validation failed")
                        .Build());
    RecordEvent(GetBeforeFillingMetricsEvent(status));
    std::move(callback).Run(std::move(validation_result));
    return;
  }

  LogJournalEvent("AttemptOtpFillingTool::OnGmailOtpConfirmationResponse",
                  JournalDetailsBuilder()
                      .Add("status", "Calling FillOtp after confirmation")
                      .Build());
  pending_cancellation_event_ =
      AttemptOtpFillingToolEvent::kCancelledDuringFillOtp;
  tool_delegate().GetActorOneTimeTokenFillingService().FillOtp(
      GetWebContentsForOtpFilling(actor_surface_handle_), trigger_field_ids_,
      std::move(otp),
      base::BindOnce(&AttemptOtpFillingTool::OnOtpFilled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::RecordEvent(AttemptOtpFillingToolEvent event) {
  RecordAttemptOtpFillingEvent(event, opt_in_state_);
}

}  // namespace actor
