// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"

#include <ostream>
#include <string_view>
#include <variant>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/to_string.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/task_id.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || \
    BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#endif

// Traits for base::ToString(). They need to be in the corresponding namespace
// of the enums.
namespace features {
std::ostream& operator<<(std::ostream& os,
                         GlicActorEnterprisePrefDefault value) {
  switch (value) {
    case GlicActorEnterprisePrefDefault::kEnabledByDefault:
      return os << "enabled_by_default";
    case GlicActorEnterprisePrefDefault::kDisabledByDefault:
      return os << "disabled_by_default";
    case GlicActorEnterprisePrefDefault::kForcedDisabled:
      return os << "forced_disabled";
  }
  return os << "unknown(" << static_cast<int>(value) << ")";
}
}  // namespace features

namespace glic {

namespace prefs {
std::ostream& operator<<(std::ostream& os,
                         GlicActuationOnWebPolicyState value) {
  switch (value) {
    case GlicActuationOnWebPolicyState::kEnabled:
      return os << "kEnabled";
    case GlicActuationOnWebPolicyState::kDisabled:
      return os << "kDisabled";
  }
  return os << "kUnknown(" << static_cast<int>(value) << ")";
}
}  // namespace prefs

std::ostream& operator<<(std::ostream& os,
                         GlicActorPolicyChecker::CanActOutcome value) {
  switch (value) {
    case GlicActorPolicyChecker::CanActOutcome::kYes:
      return os << "kYes";
    case GlicActorPolicyChecker::CanActOutcome::kNo:
      return os << "kNo";
    case GlicActorPolicyChecker::CanActOutcome::kByAllowlistOnly:
      return os << "kByAllowlistOnly";
  }
  return os << "kUnknown(" << static_cast<int>(value) << ")";
}

std::ostream& operator<<(std::ostream& os, CannotActReason value) {
  switch (value) {
    case CannotActReason::kNone:
      return os << "kNone";
    case CannotActReason::kAccountCapabilityIneligible:
      return os << "kAccountCapabilityIneligible";
    case CannotActReason::kAccountMissingChromeBenefits:
      return os << "kAccountMissingChromeBenefits";
    case CannotActReason::kDisabledByPolicy:
      return os << "kDisabledByPolicy";
    case CannotActReason::kEnterpriseWithoutManagement:
      return os << "kEnterpriseWithoutManagement";
  }
  return os << "kUnknown(" << static_cast<int>(value) << ")";
}

std::ostream& operator<<(
    std::ostream& os,
    std::variant<CannotActReason, std::string_view> value) {
  std::visit([&os](auto&& arg) { os << arg; }, value);
  return os;
}

namespace {

bool ActuationEnabledForManagedUser(Profile& profile,
                                    actor::AggregatedJournal& journal,
                                    bool emit_metric) {
  features::GlicActorEnterprisePrefDefault default_pref =
      features::kGlicActorEnterprisePrefDefault.Get();
  auto* pref_service = profile.GetPrefs();
  CHECK(pref_service);

  auto capability_pref =
      glic::prefs::GetActuationOnWebCapability(pref_service)
          .value_or(glic::prefs::GlicActuationOnWebPolicyState::kDisabled);

  bool is_enabled = false;
  if (default_pref ==
      features::GlicActorEnterprisePrefDefault::kForcedDisabled) {
    is_enabled = false;
  } else {
    is_enabled =
        capability_pref == glic::prefs::GlicActuationOnWebPolicyState::kEnabled;
  }

  // Log the behavior
  journal.Log(GURL(), actor::TaskId(), "ActuationEnabledForManagedUser",
              actor::JournalDetailsBuilder()
                  .Add("default_pref", base::ToString(default_pref))
                  .Add("capability_pref", base::ToString(capability_pref))
                  .Add("is_enabled", is_enabled)
                  .Build());

  // Emit the UMA histogram metric
  if (emit_metric) {
    base::UmaHistogramBoolean("Glic.Actor.ManagedUserActuationEnabled",
                              is_enabled);
  }

  return is_enabled;
}

bool HasUrlAllowlist(Profile& profile) {
  PrefService* pref_service = profile.GetPrefs();
  CHECK(pref_service);
  const std::string_view allowlist_pref_path =
      glic::prefs::kGlicActuationOnWebAllowedForURLs;
  if (!pref_service->HasPrefPath(allowlist_pref_path)) {
    return false;
  }
  const base::ListValue& allowlist = pref_service->GetList(allowlist_pref_path);
  return !allowlist.empty();
}

// TODO(crbug.com/471065012): This is a consumer check so it should be moved to
// the overall actuation account access check. Placed here for a quick fix.
bool AccountHasChromeBenefits(Profile& profile,
                              actor::AggregatedJournal& journal) {
  subscription_eligibility::SubscriptionEligibilityService*
      subscription_service = subscription_eligibility::
          SubscriptionEligibilityServiceFactory::GetForProfile(&profile);
  CHECK(subscription_service);
  const base::flat_set<int32_t>& eligible_tiers =
      GlicActorPolicyChecker::GetActorEligibleTiers();
  int32_t subscription_tier = subscription_service->GetAiSubscriptionTier();
  journal.Log(
      GURL(), actor::TaskId(), "AccountHasChromeBenefits",
      actor::JournalDetailsBuilder()
          .Add("subscription_tier", subscription_tier)
          .Add("eligible_tiers", features::kGlicActorEligibleTiers.Get())
          .Build());
  return eligible_tiers.contains(subscription_tier);
}

}  // namespace

GlicActorPolicyChecker::GlicActorPolicyChecker(Profile& profile)
    : profile_(&profile),
      url_blocklist_manager_(profile_->GetPrefs(),
                             glic::prefs::kGlicActuationOnWebBlockedForURLs,
                             glic::prefs::kGlicActuationOnWebAllowedForURLs),
      journal_(
          actor::ActorKeyedService::Get(profile_)->GetJournal().GetSafeRef()) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
  subscription_eligibility::SubscriptionEligibilityService*
      subscription_service = subscription_eligibility::
          SubscriptionEligibilityServiceFactory::GetForProfile(profile_);
  if (subscription_service) {
    subscription_eligibility_service_observation_.Observe(subscription_service);
  }

  std::tie(can_act_on_web_, cannot_act_on_web_reason_) =
      ComputeActOnWebCapability(/*disable_for_enterprise=*/false);

  std::tie(glic_api_can_act_on_web_, glic_api_cannot_act_on_web_reason_) =
      ComputeActOnWebCapability(/*disable_for_enterprise=*/true);

  pref_change_registrar_.Init(profile_->GetPrefs());
  // Listens to policy changes.
  pref_change_registrar_.Add(
      glic::prefs::kGlicActuationOnWeb,
      base::BindRepeating(&GlicActorPolicyChecker::OnPrefOrAccountChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  url_blocklist_subscription_ = url_blocklist_manager_.AddObserver(
      base::BindRepeating(&GlicActorPolicyChecker::OnPrefOrAccountChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  // Listens to user status changes.
  pref_change_registrar_.Add(
      glic::prefs::kGlicUserStatus,
      base::BindRepeating(&GlicActorPolicyChecker::OnPrefOrAccountChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

GlicActorPolicyChecker::~GlicActorPolicyChecker() = default;

// static
const base::flat_set<int32_t>& GlicActorPolicyChecker::GetActorEligibleTiers() {
  static const base::NoDestructor<base::flat_set<int32_t>> eligible_tiers([] {
    std::string tier_list = features::kGlicActorEligibleTiers.Get();
    std::vector<std::string_view> tier_pieces = base::SplitStringPiece(
        tier_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    base::flat_set<int32_t> tiers;
    tiers.reserve(tier_pieces.size());
    for (const auto& piece : tier_pieces) {
      int32_t tier_id = 0;
      if (base::StringToInt(piece, &tier_id)) {
        tiers.insert(tier_id);
      }
    }
    return tiers;
  }());
  return *eligible_tiers;
}

base::CallbackListSubscription
GlicActorPolicyChecker::AddActOnWebCapabilityChangedCallback(
    CanActOnWebChangedCallback callback) {
  return changed_callback_list_.Add(callback);
}

void GlicActorPolicyChecker::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      OnPrefOrAccountChanged();
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void GlicActorPolicyChecker::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager &&
      info.GetAccountId() == identity_manager->GetPrimaryAccountId(
                                 signin::ConsentLevel::kSignin)) {
    OnPrefOrAccountChanged();
  }
}

void GlicActorPolicyChecker::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager &&
      info.GetAccountId() == identity_manager->GetPrimaryAccountId(
                                 signin::ConsentLevel::kSignin)) {
    OnPrefOrAccountChanged();
  }
}

void GlicActorPolicyChecker::OnAiSubscriptionTierUpdated(
    int32_t new_subscription_tier) {
  OnPrefOrAccountChanged();
}

// static
bool GlicActorPolicyChecker::IsEnterpriseAccountForActor(
    Profile& profile,
    actor::AggregatedJournal& journal) {
  // Note: Delegated to GlicEnabling to evaluate both Workspace data protection
  // (`IsAccountDataProtected()`) and identity domain management
  // (`IsAccountManaged()`), which are backed by two different Google API
  // endpoints.
  //
  // GlicEnabling internally enforces the GlicCachedUserStatusScope LINT
  // invariants when checking user status. Both signals are extracted and
  // logged to the Actuation journal for completeness.
  bool is_enterprise_account_data_protected =
      GlicEnabling::IsAccountDataProtected(&profile);
  signin::Tribool is_managed = GlicEnabling::IsAccountManaged(&profile);

  journal.Log(GURL(), actor::TaskId(), "IsEnterpriseAccount",
              actor::JournalDetailsBuilder()
                  .Add("is_enterprise_account_data_protected",
                       base::ToString(is_enterprise_account_data_protected))
                  .Add("is_managed", signin::TriboolToString(is_managed))
                  .Build());

  return GlicEnabling::IsEnterpriseAccount(&profile);
}

// static
bool GlicActorPolicyChecker::IsBrowserManagedForActor(Profile& profile) {
  return GlicEnabling::IsBrowserManaged(&profile);
}

bool GlicActorPolicyChecker::CanActOnWeb() const {
  return can_act_on_web_ != CanActOutcome::kNo;
}

CannotActReason GlicActorPolicyChecker::CannotActOnWebReason() const {
  return cannot_act_on_web_reason_;
}

bool GlicActorPolicyChecker::GlicApiCanActOnWeb() const {
  return glic_api_can_act_on_web_ != CanActOutcome::kNo;
}

CannotActReason GlicActorPolicyChecker::GlicApiCannotActOnWebReason() const {
  return glic_api_cannot_act_on_web_reason_;
}

void GlicActorPolicyChecker::OnPrefOrAccountChanged() {
  auto old_value = can_act_on_web_;
  std::tie(can_act_on_web_, cannot_act_on_web_reason_) =
      ComputeActOnWebCapability(/*disable_for_enterprise=*/false);
  std::tie(glic_api_can_act_on_web_, glic_api_cannot_act_on_web_reason_) =
      ComputeActOnWebCapability(/*disable_for_enterprise=*/true);
  if (old_value != can_act_on_web_) {
    changed_callback_list_.Notify(CanActOnWeb());
  }
}

std::pair<GlicActorPolicyChecker::CanActOutcome, CannotActReason>
GlicActorPolicyChecker::ComputeActOnWebCapability(bool disable_for_enterprise) {
  auto log_and_return =
      [&](CanActOutcome outcome,
          std::variant<CannotActReason, std::string_view> reason) {
        CHECK(outcome == CanActOutcome::kYes ||
              std::holds_alternative<CannotActReason>(reason));
        journal_->Log(GURL(), actor::TaskId(),
                      "GlicActorPolicyChecker::ComputeActOnWebCapability",
                      actor::JournalDetailsBuilder()
                          .Add("outcome", base::ToString(outcome))
                          .Add("reasons", base::ToString(reason))
                          .Build());
        return std::pair{outcome,
                         std::holds_alternative<CannotActReason>(reason)
                             ? std::get<CannotActReason>(reason)
                             : CannotActReason::kNone};
      };

  if (features::kGlicActorPolicyControlExemption.Get()) {
    return log_and_return(
        CanActOutcome::kYes,
        "extempted via cmdline `glic_actor_policy_control_exemption`");
  }

  // If the main Glic check has been split to no longer use the
  // can_use_model_execution_features capability (see
  // kGlicEligibilitySeparateAccountCapability), then that capability must be
  // checked here. This is because actuation currently implements stricter
  // account checks.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);
  bool can_use_adult_features = GlicEnabling::CanUseAdultFeatures(
      identity_manager
          ->FindExtendedAccountInfoByAccountId(
              identity_manager
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                  .account_id)
          .GetAccountCapabilities());
  if (!can_use_adult_features) {
    return log_and_return(CanActOutcome::kNo,
                          CannotActReason::kAccountCapabilityIneligible);
  }

  bool is_likely_dogfood_client = GlicEnabling::IsLikelyDogfoodClient();
  bool is_google_internal_account =
      gaia::IsGoogleInternalAccountEmail(profile_->GetProfileUserName());
  if (is_likely_dogfood_client && is_google_internal_account) {
    return log_and_return(CanActOutcome::kYes,
                          "is likely dogfood client with google account");
  }

  // Consumer checks.

  if (IsEnterpriseAccountForActor(*profile_, *journal_)) {
    if (disable_for_enterprise) {
      // If disable_for_enterprise=true,
      // Enterprise (workspace) account is disabled for now since tier
      // information is not available in Chrome
      // TODO(b/525028864): Retrieve enterprise account tier information for
      // more accurate check.
      return log_and_return(CanActOutcome::kNo,
                            CannotActReason::kEnterpriseWithoutManagement);
    }

    if (!IsBrowserManagedForActor(*profile_)) {
      // Edge (error) case: an enterprise account without management. This means
      // that policy delivery is not trustworthy (because the policy delivery
      // over a domain requires management). Fallback to the default policy pref
      // value. This should be extremely rare.
      bool default_pref_enabled =
          features::kGlicActorEnterprisePrefDefault.Get() ==
          features::GlicActorEnterprisePrefDefault::kEnabledByDefault;
      if (default_pref_enabled) {
        return log_and_return(
            CanActOutcome::kYes,
            "Enterprise account without management: default pref enabled");
      } else {
        return log_and_return(CanActOutcome::kNo,
                              CannotActReason::kEnterpriseWithoutManagement);
      }
    }
  }

  if (IsBrowserManagedForActor(*profile_)) {
    bool policy_enabled = ActuationEnabledForManagedUser(
        *profile_, *journal_, /*emit_metrics=*/!disable_for_enterprise);
    bool has_allowlist = HasUrlAllowlist(*profile_);

    if (!policy_enabled) {
      if (has_allowlist) {
        // If actuation in general is blocked by policy, but there is a
        // non-empty allow list, then we need `CanActOnWeb()` to be true so we
        // can attempt actuation up until the point where we evaluate a URL for
        // its inclusion in the allow list. If it's not explicitly allowed by
        // the list, then we perform the blocking there.
        return log_and_return(CanActOutcome::kByAllowlistOnly,
                              CannotActReason::kDisabledByPolicy);
      }
      return log_and_return(CanActOutcome::kNo,
                            CannotActReason::kDisabledByPolicy);
    }

    // policy_enabled is true here.
    if (!disable_for_enterprise &&
        base::FeatureList::IsEnabled(
            features::
                kGlicActorWorkspaceExemptFromTierCheckRegressionFixKillswitch) &&
        IsEnterpriseAccountForActor(*profile_, *journal_)) {
      return log_and_return(
          CanActOutcome::kYes,
          "Managed: actuation enabled via policy for enterprise account");
    }

    // If they have Chrome benefits, they can act everywhere.
    if (AccountHasChromeBenefits(*profile_, *journal_)) {
      return log_and_return(
          CanActOutcome::kYes,
          "Managed: actuation enabled via policy and account has benefits");
    }

    // policy_enabled is true, but they don't have Chrome benefits.
    if (has_allowlist) {
      // Allowed on allowlisted URLs even without benefits.
      return log_and_return(CanActOutcome::kByAllowlistOnly,
                            CannotActReason::kDisabledByPolicy);
    }

    // policy_enabled is true, no benefits, no allowlist -> blocked.
    return log_and_return(CanActOutcome::kNo,
                          CannotActReason::kAccountMissingChromeBenefits);
  }

  // At this point, the account is neither enterprise nor override by policy.
  // Check Chrome benefits.
  if (AccountHasChromeBenefits(*profile_, *journal_)) {
    return log_and_return(CanActOutcome::kYes,
                          "Not managed: account has chrome benefits");
  }

  return log_and_return(CanActOutcome::kNo,
                        CannotActReason::kAccountMissingChromeBenefits);
}

GlicActorPolicyChecker::UrlBlockReason GlicActorPolicyChecker::Evaluate(
    const GURL& url) const {
  const policy::URLBlocklist::URLBlocklistState state =
      url_blocklist_manager_.GetURLBlocklistState(url);
  if (state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return UrlBlockReason::kExplicitlyBlocked;
  }
  if (state == policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST) {
    return UrlBlockReason::kExplicitlyAllowed;
  }

  // If the general policy is set to disable acting, then if the url is not in
  // the allow list, we block.
  if (can_act_on_web_ == CanActOutcome::kByAllowlistOnly) {
    return UrlBlockReason::kExplicitlyBlocked;
  }

  return UrlBlockReason::kNotBlocked;
}

base::CallbackListSubscription
GlicActorPolicyChecker::AddUrlListsUpdateObserverForTesting(
    base::RepeatingClosure callback) {
  return url_blocklist_manager_.AddObserver(std::move(callback));
}

void GlicActorPolicyChecker::ValidateContentSentToRenderer(
    content::RenderFrameHost* frame,
    const std::string& content,
    ContentValidationCallback callback) const {
  content::WebContents* web_contents =
      frame ? content::WebContents::FromRenderFrameHost(frame) : nullptr;
  if (!web_contents || !profile_) {
    std::move(callback).Run(ContentValidationReason::kAllowed);
    return;
  }

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS) || \
    BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  enterprise_data_protection::PasteFromGeminiIfAllowedByPolicy(
      frame, content,
      base::BindOnce(
          [](ContentValidationCallback cb, bool allowed) {
            std::move(cb).Run(allowed ? ContentValidationReason::kAllowed
                                      : ContentValidationReason::kBlocked);
          },
          std::move(callback)));
#else
  std::move(callback).Run(ContentValidationReason::kAllowed);
#endif
}

}  // namespace glic
