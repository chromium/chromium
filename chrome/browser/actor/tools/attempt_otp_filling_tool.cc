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

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/shared_types.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/match_type.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/render_frame_host.h"

namespace actor {

namespace {

constexpr base::TimeDelta kGmailOtpOptInCoolOffPeriod = base::Days(90);

void OnOtpFrameOriginMatchEvaluated(
    bool should_use_strong_matching,
    base::OnceCallback<void(bool)> callback,
    std::optional<affiliations::MatchType> match_type) {
  if (!match_type.has_value()) {
    std::move(callback).Run(false);
    return;
  }

  // Exact or affiliated matches are always allowed.
  bool is_exact_or_affiliated =
      (*match_type == affiliations::MatchType::kExact) ||
      (static_cast<int>(*match_type) &
       static_cast<int>(affiliations::MatchType::kAffiliated));
  if (is_exact_or_affiliated) {
    std::move(callback).Run(true);
    return;
  }

  // PSL match is only allowed when `should_use_strong_matching` is false.
  bool is_psl = static_cast<int>(*match_type) &
                static_cast<int>(affiliations::MatchType::kPSL);
  std::move(callback).Run(is_psl && !should_use_strong_matching);
}

// We need to make sure that we don't skip user confirmation for OTPs that do
// not belong to actor login flows. Actor login fills credentials in all iframes
// that it considers trustworthy because it doesn't know which one contains the
// correct login form. It also uses 2 different trust levels (based on user
// permission type), both are based on iframe's and main frame's origins.
// This method needs to match the same trust levels, hence the
// `should_use_strong_matching` parameter. To avoid checking each filled
// frame, we try to match the OTP form's origin with the origin of the main
// frame where actor login flow started and rely on the fact that affiliations
// are transitive.
void VerifyOtpFrameOriginMatch(affiliations::DomainRelationChecker* checker,
                               const url::Origin& login_main_frame_origin,
                               const url::Origin& otp_origin,
                               bool should_use_strong_matching,
                               base::OnceCallback<void(bool)> callback) {
  if (!checker) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        // Without the affiliation information, only origin equality is checked.
        base::BindOnce(std::move(callback),
                       login_main_frame_origin.IsSameOriginWith(otp_origin)));
    return;
  }
  checker->Check(
      login_main_frame_origin, otp_origin,
      base::BindOnce(&OnOtpFrameOriginMatchEvaluated,
                     should_use_strong_matching, std::move(callback)));
}

// Returns the `RenderFrameHost` containing the OTP fields.
content::RenderFrameHost* GetOtpFrame(
    tabs::TabHandle tab_handle,
    base::span<const PageTarget> trigger_fields) {
  if (trigger_fields.empty()) {
    return nullptr;
  }
  return FindTargetLocalRootFrame(tab_handle, trigger_fields[0]);
}

// Checks if the tool execution corresponds to an actor login's sign in flow.
// This is used to determine if we can skip the confirmation UI. The
// reasoning being that the user already consented to the login attempt
// during actor login execution and this OTP filling is considered part of
// the same flow.
//
// The verification consists of the following checks:
// 1. In `IsActorLoginFlow`:
//    - Verify the target OTP frame was tracked during the login attempt
//      (checked via `navigations_per_frame` in `ActorLoginContext`).
//    - Verify that no tracked login frames have navigated more than once
//      (multiple redirects likely exit the sign-in flow).
// 2. In `VerifyOtpFrameOriginMatch` (and `OnOtpFrameOriginMatchEvaluated`):
//    - Verify the OTP frame origin is related to the main frame
//      origin of the login attempt (via `DomainRelationChecker`).
//    - Verify the match strength complies with `should_use_strong_matching`
//      (rejecting grouped affiliations, and allowing PSL only for weak
//      matching).
// TODO(crbug.com/530490937): Consider extracting this and other functions into
// a separate class.
void IsActorLoginFlow(affiliations::DomainRelationChecker* checker,
                      content::RenderFrameHost& otp_frame,
                      autofill::ActorLoginContext context,
                      base::OnceCallback<void(bool)> callback) {
  content::FrameTreeNodeId otp_frame_id = otp_frame.GetFrameTreeNodeId();
  if (!context.navigations_per_frame.contains(otp_frame_id)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Actor Login filled credentials in all of these frames but the actual
  // login frame is unknown. While finding an OTP field in one
  // of those frames is a signal that the frame was the login frame, it's not
  // guaranteed. Therefore, require all frames to have <2 navigations to
  // avoid accidentally skipping user confirmation for OTPs not meant for
  // login flows.
  bool navigations_ok = std::ranges::all_of(
      context.navigations_per_frame,
      [](const std::pair<const content::FrameTreeNodeId, int>& entry) {
        return entry.second < 2;
      });
  if (!navigations_ok) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Last check: verify OTP form origin and main frame origin are related.
  VerifyOtpFrameOriginMatch(
      checker, context.origin, otp_frame.GetLastCommittedOrigin(),
      context.should_use_strong_matching, std::move(callback));
}

// Returns the `mojom::ActionResultPtr` for a given
// `FormFillingContextStatus`.
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

}  // namespace

AttemptOtpFillingTool::AttemptOtpFillingTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    tabs::TabHandle tab_handle,
    std::vector<PageTarget> trigger_fields,
    bool for_signin)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab_handle),
      trigger_fields_(std::move(trigger_fields)),
      for_signin_(for_signin) {
  // Guaranteed by validation in `CreateAttemptOtpFillingRequest` in
  // `actor_proto_conversion.cc`.
  CHECK(!trigger_fields_.empty());

  auto* affiliation_service =
      AffiliationServiceFactory::GetForProfile(&tool_delegate.GetProfile());
  if (affiliation_service) {
    domain_relation_checker_ =
        std::make_unique<affiliations::DomainRelationChecker>(
            *affiliation_service);
  }
}

AttemptOtpFillingTool::~AttemptOtpFillingTool() = default;

void AttemptOtpFillingTool::Validate(ToolCallback callback) {
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
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kFormFillingAutofillUnavailable,
                   /*requires_page_stabilization=*/false,
                   "Gmail OTP disabled and within cool-off period for Gmail "
                   "OTP opt-in dialog."));
  } else {
    tool_delegate().RequestToShowGmailOtpOptInDialog(
        base::BindOnce(&AttemptOtpFillingTool::OnGmailOtpOptInResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void AttemptOtpFillingTool::OnGmailOtpOptInResponse(
    ToolCallback callback,
    webui::mojom::GmailOtpOptInResultPtr response) {
  if (!response || response.is_null()) {
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kFormFillingDialogError,
                   /*requires_page_stabilization=*/false,
                   "Gmail OTP opt-in dialog response is null"));
    return;
  }

  if (response->is_error_reason()) {
    LogJournalEvent("AttemptOtpFillingTool::OnGmailOtpOptInResponse",
                    JournalDetailsBuilder()
                        .Add("error_reason", response->get_error_reason())
                        .Build());
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kFormFillingDialogError,
                   /*requires_page_stabilization=*/false,
                   "Error in Gmail OTP opt-in dialog response"));
    return;
  }

  bool opt_in_permission_granted = response->get_permission_granted();

  PrefService* prefs = tool_delegate().GetProfile().GetPrefs();
  if (!opt_in_permission_granted) {
    autofill::prefs::SetAutofillGmailOtpFillingActivationDismissalTimestamp(
        prefs, base::Time::Now());
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kFormFillingAutofillUnavailable,
                   /*requires_page_stabilization=*/false,
                   "User declined Gmail OTP opt-in."));
    return;
  }

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
    return MakeResult(mojom::ActionResultCode::kTabWentAway,
                      /*requires_page_stabilization=*/false,
                      "Target tab was destroyed before invocation.");
  }

  if (!last_observation) {
    return MakeResult(mojom::ActionResultCode::kFormFillingNoLastTabObservation,
                      /*requires_page_stabilization=*/false,
                      "Last tab observation is null.");
  }

  trigger_field_ids_.clear();
  trigger_field_ids_.reserve(trigger_fields_.size());
  for (const auto& trigger_field : trigger_fields_) {
    autofill::FieldGlobalId field_id =
        GetFieldIdFromPageTarget(last_observation, tab, trigger_field);
    if (!field_id) {
      return MakeResult(mojom::ActionResultCode::kFormFillingFieldNotFound,
                        /*requires_page_stabilization=*/false,
                        "Trigger field not found.");
    }
    trigger_field_ids_.push_back(field_id);
  }

  return GetResultFromFormFillingStatus(
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ValidateFormFillingContext(GetTargetTab(), trigger_field_ids_));
}

void AttemptOtpFillingTool::Invoke(ToolCallback callback) {
  LogJournalEvent("AttemptOtpFillingTool::Invoke",
                  JournalDetailsBuilder()
                      .Add("trigger_fields_count", trigger_field_ids_.size())
                      .Add("for_signin", for_signin_)
                      .Build());

  content::RenderFrameHost* otp_frame =
      GetOtpFrame(GetTargetTab(), trigger_fields_);
  if (!otp_frame) {
    LogJournalEvent("AttemptOtpFillingTool::Invoke",
                    JournalDetailsBuilder()
                        .Add("error", "No frame containing an OTP")
                        .Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            // TODO(crbug.com/502908360): Consider using a more specific error
            // code.
            MakeResult(mojom::ActionResultCode::kFormFillingFieldNotFound,
                       /*requires_page_stabilization=*/false,
                       "Target frame containing OTP fields not found.")));
    return;
  }

  // Consume the context. The service clears its state and stops observing.
  std::optional<autofill::ActorLoginContext> context =
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ConsumeLoginContext();

  if (context.has_value()) {
    IsActorLoginFlow(
        domain_relation_checker_.get(), *otp_frame, std::move(*context),
        base::BindOnce(&AttemptOtpFillingTool::OnActorLoginFlowChecked,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AttemptOtpFillingTool::OnActorLoginFlowChecked,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       /*is_actor_login=*/false));
  }
}

void AttemptOtpFillingTool::OnActorLoginFlowChecked(ToolCallback callback,
                                                    bool is_actor_login) {
  LogJournalEvent(
      "AttemptOtpFillingTool::OnActorLoginFlowChecked",
      JournalDetailsBuilder().Add("is_actor_login", is_actor_login).Build());

  if (is_actor_login) {
    // Verified sign-in journey: proceed with silent OTP filling.
    tool_delegate().GetActorOneTimeTokenFillingService().RetrieveOtp(
        GetTargetTab(), trigger_field_ids_,
        base::BindOnce(&AttemptOtpFillingTool::OnOtpRetrieved,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    LogJournalEvent("AttemptOtpFillingTool::OnActorLoginFlowChecked",
                    JournalDetailsBuilder()
                        .Add("error", "Not an Actor Login flow")
                        .Build());
    // No recent login, origin mismatch, untracked frame, or sequence broken
    // by too many navigations: require confirmation UI (Post-MVP).
    // TODO(crbug.com/504573041): Implement confirmation UI.
    std::move(callback).Run(
        MakeResult(mojom::ActionResultCode::kOtpSigninContextMismatch,
                   /*requires_page_stabilization=*/false,
                   "Silent OTP filling is only allowed in the context of actor "
                   "login flows."));
  }
}

void AttemptOtpFillingTool::OnOtpRetrieved(
    ToolCallback callback,
    base::expected<std::string, one_time_tokens::OneTimeTokenRetrievalError>
        result) {
  LogJournalEvent(
      "AttemptOtpFillingTool::OnOtpRetrieved",
      JournalDetailsBuilder().Add("otp_received", result.has_value()).Build());

  if (!result.has_value()) {
    mojom::ActionResultCode code = mojom::ActionResultCode::kOtpRetrievalError;
    std::string message = "An error occurred during OTP retrieval.";

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
      default:
        break;
    }
    std::move(callback).Run(
        MakeResult(code, /*requires_page_stabilization=*/false, message));
    return;
  }

  mojom::ActionResultPtr validation_result = GetResultFromFormFillingStatus(
      tool_delegate()
          .GetActorOneTimeTokenFillingService()
          .ValidateFormFillingContext(GetTargetTab(), trigger_field_ids_));
  if (!IsOk(*validation_result)) {
    std::move(callback).Run(std::move(validation_result));
    return;
  }

  tool_delegate().GetActorOneTimeTokenFillingService().FillOtp(
      GetTargetTab(), trigger_field_ids_, result.value(),
      base::BindOnce(&AttemptOtpFillingTool::OnOtpFilled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AttemptOtpFillingTool::OnOtpFilled(ToolCallback callback, bool success) {
  LogJournalEvent("AttemptOtpFillingTool::OnOtpFilled",
                  JournalDetailsBuilder().Add("success", success).Build());

  if (success) {
    std::move(callback).Run(MakeOkResult());
  } else {
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

}  // namespace actor
