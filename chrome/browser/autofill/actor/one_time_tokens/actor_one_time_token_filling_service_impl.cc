// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_metrics.h"
#include "chrome/browser/autofill/one_time_token_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_client_provider.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "components/actor/core/actor_switches.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/match_type.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/actor/actor_filling_observer.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"
#include "components/autofill/core/common/form_data.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"
#include "url/url_canon.h"
#include "url/url_constants.h"

namespace autofill {

using enum ActorOneTimeTokenFillingServiceRetrieveOtp;

using ::one_time_tokens::OneTimeTokenRetrievalError;

namespace {


std::string ExtractEmailDomain(std::string_view email) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      email, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() == 2) {
    return std::string(parts[1]);
  }
  return std::string();
}

// Retrieves the `AutofillManager` of the `tab`'s primary main frame.
[[nodiscard]] base::expected<std::reference_wrapper<BrowserAutofillManager>,
                             ActorFormFillingError>
GetAutofillManager(const tabs::TabInterface& tab) {
  using enum ActorFormFillingError;

  Profile* const profile =
      Profile::FromBrowserContext(tab.GetContents()->GetBrowserContext());
  if (!profile) {
    return base::unexpected(kAutofillNotAvailable);
  }
  if (AutofillClientProviderFactory::GetForProfile(profile)
          .uses_platform_autofill()) {
    // This is currently only possible on Android platforms, but this check
    // guards against this becoming applicable for Desktop platforms as well.
    // It is a requirement for the cast to `BrowserAutofillManager` to be
    // safe.
    return base::unexpected(kAutofillNotAvailable);
  }

  ContentAutofillClient* const client =
      ContentAutofillClient::FromWebContents(tab.GetContents());
  if (!client) {
    return base::unexpected(kAutofillNotAvailable);
  }
  if (AutofillManager* autofill_manager =
          client->GetAutofillManagerForPrimaryMainFrame()) {
    return *static_cast<BrowserAutofillManager*>(autofill_manager);
  }
  return base::unexpected(kAutofillNotAvailable);
}

}  // namespace

ActorOneTimeTokenFillingServiceImpl::ActorOneTimeTokenFillingServiceImpl(
    Profile* profile)
    : profile_(profile),
      domain_relation_checker_(
          std::make_unique<affiliations::DomainRelationChecker>(
              CHECK_DEREF(AffiliationServiceFactory::GetForProfile(profile)))) {
}

ActorOneTimeTokenFillingServiceImpl::~ActorOneTimeTokenFillingServiceImpl() =
    default;

void ActorOneTimeTokenFillingServiceImpl::OnPasswordFillingStarted(
    tabs::TabHandle tab_handle,
    const url::Origin& origin,
    bool should_use_strong_matching,
    base::span<const int> global_frame_ids) {
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab || !tab->GetContents()) {
    return;
  }
  content::WebContents* contents = tab->GetContents();
  content::FrameTreeNodeId sign_in_main_frame_id =
      contents->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  std::vector<std::pair<content::FrameTreeNodeId, int>> initial_navigations =
      base::ToVector(global_frame_ids, [](int frame_id) {
        return std::pair(content::FrameTreeNodeId(frame_id), 0);
      });
  initial_navigations.emplace_back(sign_in_main_frame_id, 0);
  active_login_context_ = {origin, should_use_strong_matching,
                           base::flat_map<content::FrameTreeNodeId, int>(
                               std::move(initial_navigations))};
  Observe(contents);
}

void ActorOneTimeTokenFillingServiceImpl::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!active_login_context_.has_value() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }
  content::FrameTreeNodeId navigating_id = handle->GetFrameTreeNodeId();
  auto it = active_login_context_->navigations_per_frame.find(navigating_id);
  if (it != active_login_context_->navigations_per_frame.end()) {
    it->second++;
  }
}

void ActorOneTimeTokenFillingServiceImpl::AbortLoginTracking() {
  active_login_context_ = std::nullopt;
  Observe(nullptr);
}

std::optional<ActorLoginContext>
ActorOneTimeTokenFillingServiceImpl::ConsumeLoginContext() {
  Observe(nullptr);
  return std::exchange(active_login_context_, std::nullopt);
}

void ActorOneTimeTokenFillingServiceImpl::RetrieveOtp(
    const tabs::TabHandle tab_handle,
    const url::Origin& otp_frame_origin,
    const std::vector<FieldGlobalId>& trigger_field_ids,
    bool is_login_flow,
    base::OnceCallback<void(
        base::expected<std::string, OneTimeTokenRetrievalError>)> callback) {
  RecordActorOneTimeTokenFillingServiceRetrieveOtp(kStart);
  otp_frame_origin_ = otp_frame_origin;
  is_login_flow_ = is_login_flow;

  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab || !tab->GetContents()) {
    RecordActorOneTimeTokenFillingServiceRetrieveOtp(kNullTab);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(OneTimeTokenRetrievalError::kGmailOtpUnknown)));
    return;
  }

  std::string mock_otp =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ::actor::switches::kAttemptOtpFillingMockGmailOtpValue);
  if (!mock_otp.empty()) {
    RecordActorOneTimeTokenFillingServiceRetrieveOtp(kMockOtp);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(mock_otp)));
    return;
  }

  if (otp_frame_origin_.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(OneTimeTokenRetrievalError::kGmailOtpUnknown)));
    return;
  }

  // TODO(b/502907994): Do we want to check for incognito profiles here?
  // Gemini should not be available in incognito, but should we check just to
  // be sure (and future-proof)?
  one_time_tokens::OneTimeTokenService* service =
      OneTimeTokenServiceFactory::GetForProfile(profile_);
  if (!service) {
    RecordActorOneTimeTokenFillingServiceRetrieveOtp(kNoService);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(
                OneTimeTokenRetrievalError::kGmailOtpBackendApiNotAvailable)));
    return;
  }

  // If there is a pending request, its callback is superseded. We run the
  // previous callback with a default error so the old caller can gracefully
  // time out rather than hanging indefinitely.
  RecordActorOtpRetrieveOtpCallbackSuperseded(
      ActorOtpRetrieveOtpCallbackSuperseded::kRetrieveOtpStarted);
  if (retrieve_otp_callback_) {
    RecordActorOtpRetrieveOtpCallbackSuperseded(
        ActorOtpRetrieveOtpCallbackSuperseded::kCallbackSuperseded);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(retrieve_otp_callback_),
            base::unexpected(OneTimeTokenRetrievalError::kGmailOtpUnknown)));
  }

  retrieve_otp_weak_ptr_factory_.InvalidateWeakPtrs();

  pending_sender_domain_checks_ = 0;
  subscription_timed_out_ = false;

  retrieve_otp_callback_ = std::move(callback);

  // Note: OneTimeTokenService caches tokens for 3 minutes. It does not clear
  // them upon use. If a user triggers a "Resend OTP" flow within those 3
  // minutes, this will return the originally cached token rather than waiting
  // for the new one. This relies on the assumption that previously sent tokens
  // typically remain valid for the duration of the cache.
  std::vector<one_time_tokens::OneTimeToken> cached_tokens;
  for (const auto& token : service->GetCachedOneTimeTokens()) {
    if (token.type() == one_time_tokens::OneTimeTokenType::kGmail) {
      cached_tokens.push_back(token);
    }
  }

  // The cache checking is async, so also listen to the service in the meantime
  // in case the matching token is not in the cache. The tokens arriving from
  // the service are also checked for relevance.
  SubscribeForOneTimeToken();

  if (cached_tokens.empty()) {
    return;
  }

  std::ranges::sort(cached_tokens, [](const auto& lhs, const auto& rhs) {
    return lhs.on_device_arrival_time() > rhs.on_device_arrival_time();
  });

  CheckCachedTokenMatch(std::move(cached_tokens), /*index=*/0);
}

void ActorOneTimeTokenFillingServiceImpl::SubscribeForOneTimeToken() {
  one_time_tokens::OneTimeTokenService* service =
      OneTimeTokenServiceFactory::GetForProfile(profile_);
  // The subscription comes after the cache is retrieved from the
  // service so it's obviously not null.
  CHECK(service);
  // Subscribe to OneTimeTokenService with configurable period.
  base::TimeDelta subscription_period =
      one_time_tokens::features::kGmailOtpSubscriptionPeriodParam.Get();
  subscription_ = service->Subscribe(
      one_time_tokens::OneTimeTokenSource::kGmail,
      base::Time::Now() + subscription_period,
      base::BindRepeating(
          &ActorOneTimeTokenFillingServiceImpl::OnOneTimeTokenReceived,
          retrieve_otp_weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &ActorOneTimeTokenFillingServiceImpl::OnOneTimeTokenTimeout,
          retrieve_otp_weak_ptr_factory_.GetWeakPtr()));
}

void ActorOneTimeTokenFillingServiceImpl::CheckSenderDomainMatchesFrameToFill(
    std::string_view sender_address,
    base::OnceCallback<void(std::optional<affiliations::MatchType>)> callback) {
  pending_sender_domain_checks_++;
  std::string sender_domain = ExtractEmailDomain(sender_address);
  domain_relation_checker_->Check(
      otp_frame_origin_.GetTupleOrPrecursorTupleIfOpaque(),
      url::SchemeHostPort(url::kHttpsScheme, std::move(sender_domain),
                          url::DefaultPortForScheme(url::kHttpsScheme)),
      std::move(callback));
}

void ActorOneTimeTokenFillingServiceImpl::CheckCachedTokenMatch(
    std::vector<one_time_tokens::OneTimeToken> cached_tokens,
    size_t index) {
  // If a racing check found a match already it would have invalidated
  // all the weak pointers for the other checks including this one, so this
  // wouldn't be called.
  CHECK(retrieve_otp_callback_);
  if (index >= cached_tokens.size()) {
    return;
  }

  std::string sender_address =
      cached_tokens.at(index).sender_address().value_or("");
  CheckSenderDomainMatchesFrameToFill(
      sender_address,
      base::BindOnce(
          &ActorOneTimeTokenFillingServiceImpl::OnCachedTokenMatchChecked,
          retrieve_otp_weak_ptr_factory_.GetWeakPtr(), std::move(cached_tokens),
          index));
}

bool ActorOneTimeTokenFillingServiceImpl::IsMatchTypeAllowed(
    std::optional<affiliations::MatchType> match_type) const {
  if (!match_type.has_value()) {
    return false;
  }
  bool is_exact_or_affiliated =
      (*match_type == affiliations::MatchType::kExact) ||
      (static_cast<int>(*match_type) &
       static_cast<int>(affiliations::MatchType::kAffiliated));
  if (is_exact_or_affiliated) {
    return true;
  }
  bool is_psl = static_cast<int>(*match_type) &
                static_cast<int>(affiliations::MatchType::kPSL);
  return is_psl && is_login_flow_;
}

void ActorOneTimeTokenFillingServiceImpl::OnCachedTokenMatchChecked(
    std::vector<one_time_tokens::OneTimeToken> cached_tokens,
    size_t index,
    std::optional<affiliations::MatchType> match_type) {
  // If a racing check found a match already it would have invalidated
  // all weak pointers for other checks so this wouldn't be called.
  CHECK(retrieve_otp_callback_);

  // Decrement early to ensure the counter stays reliably accurate regardless of
  // whether the match succeeds or fails. If a match is found, weak pointers are
  // synchronously invalidated below, preventing any artificial timeout races.
  pending_sender_domain_checks_--;

  if (IsMatchTypeAllowed(match_type)) {
    subscription_ = {};
    retrieve_otp_weak_ptr_factory_.InvalidateWeakPtrs();
    RecordActorOneTimeTokenFillingServiceRetrieveOtp(kSuccessCacheMatchFound);
    std::move(retrieve_otp_callback_).Run(cached_tokens.at(index).value());
    return;
  }

  CheckCachedTokenMatch(std::move(cached_tokens), index + 1);
  MaybeFailWithTimeoutError();
}

void ActorOneTimeTokenFillingServiceImpl::OnOneTimeTokenReceived(
    one_time_tokens::OneTimeTokenSource source,
    base::expected<one_time_tokens::OneTimeToken, OneTimeTokenRetrievalError>
        result) {
  // If a racing check found a match already it would have invalidated
  // the weak pointer for the on-token-received callback and this wouldn't be
  // called.
  CHECK(retrieve_otp_callback_);

  if (!result.has_value()) {
    retrieve_otp_weak_ptr_factory_.InvalidateWeakPtrs();
    RecordActorOneTimeTokenFillingServiceRetrieveOtp(kError);
    std::move(retrieve_otp_callback_).Run(base::unexpected(result.error()));
    return;
  }

  std::string sender_address = result->sender_address().value_or("");
  CheckSenderDomainMatchesFrameToFill(
      sender_address,
      base::BindOnce(
          &ActorOneTimeTokenFillingServiceImpl::OnReceivedTokenMatchChecked,
          retrieve_otp_weak_ptr_factory_.GetWeakPtr(), std::move(*result)));
}

void ActorOneTimeTokenFillingServiceImpl::OnReceivedTokenMatchChecked(
    one_time_tokens::OneTimeToken token,
    std::optional<affiliations::MatchType> match_type) {
  // If a previous check found a match already it would have invalidated
  // the weak pointer for this callback, so this wouldn't be called.
  CHECK(retrieve_otp_callback_);

  // Decrement early to ensure the counter stays reliably accurate regardless of
  // whether the match succeeds or fails. If a match is found, weak pointers are
  // synchronously invalidated below, preventing any artificial timeout races.
  pending_sender_domain_checks_--;

  if (IsMatchTypeAllowed(match_type)) {
    subscription_ = {};
    retrieve_otp_weak_ptr_factory_.InvalidateWeakPtrs();
    RecordActorOneTimeTokenFillingServiceRetrieveOtp(
        kSuccessReceivedMatchFound);
    std::move(retrieve_otp_callback_).Run(token.value());
    return;
  }

  MaybeFailWithTimeoutError();
}

void ActorOneTimeTokenFillingServiceImpl::OnOneTimeTokenTimeout() {
  subscription_timed_out_ = true;
  MaybeFailWithTimeoutError();
}

// TODO(b:526619811): This returns an error from `OneTimeTokenRetrievalError`,
// this will be fixed soon. We are planning on moving the whole retrieval out of
// this class.
void ActorOneTimeTokenFillingServiceImpl::MaybeFailWithTimeoutError() {
  if (subscription_timed_out_ && pending_sender_domain_checks_ == 0) {
    CHECK(retrieve_otp_callback_);
    retrieve_otp_weak_ptr_factory_.InvalidateWeakPtrs();
    RecordActorOneTimeTokenFillingServiceRetrieveOtp(kRetrievalTimeout);
    std::move(retrieve_otp_callback_)
        .Run(
            base::unexpected(OneTimeTokenRetrievalError::kSubscriptionExpired));
  }
}

void ActorOneTimeTokenFillingServiceImpl::FillOtp(
    const tabs::TabHandle tab_handle,
    const std::vector<FieldGlobalId>& trigger_field_ids,
    const std::string& otp,
    base::OnceCallback<void(bool)> callback) {
  using enum ActorOneTimeTokenFillingServiceFillOtp;
  RecordActorOneTimeTokenFillingServiceFillOtp(kStart);
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab || !tab->GetContents()) {
    RecordActorOneTimeTokenFillingServiceFillOtp(kNullTab);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  if (trigger_field_ids.empty()) {
    RecordActorOneTimeTokenFillingServiceFillOtp(kEmptyTriggerFieldIds);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // ActorFillingObserver::Activate only supports one callback at a time. If
  // FillOtp is called while another filling operation is still in progress,
  // the previous callback would be overwritten and lost. Assuming the Actor
  // coordinates sequential usage, concurrent calls are unexpected.
  if (filling_observer_) {
    RecordActorOneTimeTokenFillingServiceFillOtp(kConcurrentCall);
    LOG(WARNING) << "FillOtp called while another filling operation is still "
                    "in progress. The new request is ignored.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Get the BrowserAutofillManager associated with the tab's primary main
  // frame.
  base::expected<std::reference_wrapper<BrowserAutofillManager>,
                 ActorFormFillingError>
      maybe_manager = GetAutofillManager(*tab);
  if (!maybe_manager.has_value()) {
    RecordActorOneTimeTokenFillingServiceFillOtp(kNoAutofillManager);
    LOG(WARNING) << "FillOtp failed: AutofillManager not available.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  BrowserAutofillManager& autofill_manager = maybe_manager.value();

  // We only use the first trigger field ID. This is based on the assumption
  // that all trigger fields belong to the same form. The actual mapping of the
  // OTP to potentially multiple fields is handled downstream by
  // `CreateFillDataForOtpSuggestion`, which examines the entire form structure.
  const FieldGlobalId& trigger_field_id = trigger_field_ids.front();

  // Find the cached form structure and field using the first trigger field ID.
  const FormStructure* const form_structure =
      autofill_manager.FindCachedFormById(trigger_field_id);
  if (!form_structure) {
    RecordActorOneTimeTokenFillingServiceFillOtp(kFormStructureNotFound);
    LOG(WARNING) << "FillOtp failed: Form structure containing trigger field "
                 << trigger_field_id << " not found in cache.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  const AutofillField* const autofill_field =
      form_structure->GetFieldById(trigger_field_id);
  if (!autofill_field) {
    RecordActorOneTimeTokenFillingServiceFillOtp(kTriggerFieldNotFound);
    LOG(WARNING) << "FillOtp failed: Trigger field " << trigger_field_id
                 << " not found in the form structure.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // OTPs are sometimes split across multiple single-digit input fields.
  // `CreateFillDataForOtpSuggestion` maps the OTP value to the appropriate
  // fields in the form.
  OtpFillData otp_fill_data = CreateFillDataForOtpSuggestion(
      *form_structure, *autofill_field, base::UTF8ToUTF16(otp));

  if (otp_fill_data.empty()) {
    RecordActorOneTimeTokenFillingServiceFillOtp(kEmptyFillData);
    LOG(WARNING) << "FillOtp failed: Generated OtpFillData is empty.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // The `ActorFillingObserver` monitors the actual filling of fields in the
  // renderer and notifies the service when it completes or times out.
  // We have already verified that `!filling_observer_` holds true at the
  // entry of this method.
  filling_observer_ =
      std::make_unique<ActorFillingObserver>(autofill_manager.client());

  // Identify all fields that are expected to be filled to inform the observer.
  std::vector<FieldGlobalId> filled_field_ids = base::ToVector(
      otp_fill_data, [](const auto& pair) { return pair.first; });
  filling_observer_->ObserveNewFilling(filled_field_ids);

  // Trigger the filling operation through the Autofill manager.
  autofill_manager.FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form_structure->global_id(),
      trigger_field_id, &otp_fill_data, AutofillTriggerSource::kGlic,
      /*blocked_fields=*/{});

  // Activate the observer and wait for completion.
  filling_observer_->Activate(base::BindOnce(
      [](base::WeakPtr<ActorOneTimeTokenFillingServiceImpl> service,
         base::OnceCallback<void(bool)> callback,
         base::expected<base::flat_map<FieldGlobalId, std::string>,
                        ActorFormFillingError> result) {
        using enum ActorOneTimeTokenFillingServiceFillOtp;
        if (result.has_value()) {
          RecordActorOneTimeTokenFillingServiceFillOtp(kSuccess);
        } else {
          RecordActorOneTimeTokenFillingServiceFillOtp(kError);
        }
        // Once the filling operation completes or times out, we must reset the
        // observer to clear the busy state so subsequent OTP filling requests
        // can proceed.
        // We use `DeleteSoon` to destroy the observer asynchronously after this
        // callback completes to avoid potential self-deletion/lifetime issues
        // if the callback is triggered synchronously (leaving the observer
        // in the call stack).
        if (service && service->filling_observer_) {
          base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
              FROM_HERE, std::move(service->filling_observer_));
        }
        std::move(callback).Run(result.has_value());
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

FormFillingContextStatus
ActorOneTimeTokenFillingServiceImpl::ValidateFormFillingContext(
    tabs::TabHandle tab_handle,
    base::span<const FieldGlobalId> trigger_field_ids) const {
  CHECK(!trigger_field_ids.empty());
  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab || !tab->GetContents()) {
    return FormFillingContextStatus::kTabNotAvailable;
  }

  content::WebContents* web_contents = tab->GetContents();
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  if (!helper) {
    return FormFillingContextStatus::kInsecureContext;
  }

  const security_state::SecurityLevel security_level =
      helper->GetSecurityLevel();
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();

  // Verify that the page in `web_contents` is loaded over cryptographic HTTPS
  // and has a valid certificate without mixed content.
  if (!entry || !entry->GetURL().SchemeIsCryptographic() ||
      !security_state::IsSslCertificateValid(security_level)) {
    return FormFillingContextStatus::kInsecureContext;
  }

  base::expected<std::reference_wrapper<BrowserAutofillManager>,
                 ActorFormFillingError>
      maybe_manager = GetAutofillManager(*tab);
  if (!maybe_manager.has_value()) {
    return FormFillingContextStatus::kFormNotFound;
  }
  BrowserAutofillManager& autofill_manager = maybe_manager.value();

  // Find `form_structure` in `autofill_manager` using the first ID in
  // `trigger_field_ids`.
  const FormStructure* const form_structure =
      autofill_manager.FindCachedFormById(trigger_field_ids.front());
  if (!form_structure) {
    return FormFillingContextStatus::kFormNotFound;
  }

  // Ensure `form_structure` does not submit to an insecure mixed content
  // action.
  if (autofill::IsFormMixedContent(autofill_manager.client(),
                                   form_structure->ToFormData())) {
    return FormFillingContextStatus::kInsecureContext;
  }

  return FormFillingContextStatus::kSecure;
}

base::WeakPtr<ActorOneTimeTokenFillingService>
ActorOneTimeTokenFillingServiceImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
