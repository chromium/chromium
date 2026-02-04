// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"

#include <ostream>
#include <string_view>
#include <variant>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/to_string.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"

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
}

std::ostream& operator<<(std::ostream& os,
                         GlicActorPolicyChecker::CannotActReason value) {
  switch (value) {
    case GlicActorPolicyChecker::CannotActReason::kNone:
      return os << "kNone";
    case GlicActorPolicyChecker::CannotActReason::kAccountCapabilityIneligible:
      return os << "kAccountCapabilityIneligible";
    case GlicActorPolicyChecker::CannotActReason::kAccountMissingChromeBenefits:
      return os << "kAccountMissingChromeBenefits";
    case GlicActorPolicyChecker::CannotActReason::kDisabledByPolicy:
      return os << "kDisabledByPolicy";
    case GlicActorPolicyChecker::CannotActReason::kEnterpriseWithoutManagement:
      return os << "kEnterpriseWithoutManagement";
  }
}

std::ostream& operator<<(std::ostream& os,
                         std::variant<GlicActorPolicyChecker::CannotActReason,
                                      std::string_view> value) {
  std::visit([&os](auto&& arg) { os << arg; }, value);
  return os;
}

namespace {

bool IsLikelyDogfoodClient() {
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  return variations_service && variations_service->IsLikelyDogfoodClient();
}

bool IsBrowserManaged(Profile& profile) {
  auto* management_service_factory =
      policy::ManagementServiceFactory::GetInstance();
  auto* browser_management_service =
      management_service_factory->GetForProfile(&profile);
  return browser_management_service && browser_management_service->IsManaged();
}

bool ActuationEnabledForManagedUser(Profile& profile,
                                    actor::AggregatedJournal& journal) {
  features::GlicActorEnterprisePrefDefault default_pref =
      features::kGlicActorEnterprisePrefDefault.Get();
  auto* pref_service = profile.GetPrefs();
  CHECK(pref_service);
  auto capability_pref =
      static_cast<glic::prefs::GlicActuationOnWebPolicyState>(
          pref_service->GetInteger(glic::prefs::kGlicActuationOnWeb));
  journal.Log(GURL(), actor::TaskId(), "ActuationEnabledForManagedUser",
              actor::JournalDetailsBuilder()
                  .Add("default_pref", base::ToString(default_pref))
                  .Add("capability_pref", base::ToString(capability_pref))
                  .Build());
  if (default_pref ==
      features::GlicActorEnterprisePrefDefault::kForcedDisabled) {
    return false;
  }
  return capability_pref ==
         glic::prefs::GlicActuationOnWebPolicyState::kEnabled;
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

bool IsEnterpriseAccount(Profile& profile, actor::AggregatedJournal& journal) {
  // Note: both `is_enterprise_account_data_protected` and
  // `AccountInfo::IsManaged()` check for Workspace accounts. They are backed
  // by two different Google API endpoints. Both are checked for completeness.

  bool is_enterprise_account_data_protected = false;
  // Ensure that assumptions about when we do or do not update the cached user
  // status are not broken.
  // LINT.IfChange(GlicCachedUserStatusScope)
  if (base::FeatureList::IsEnabled(features::kGlicUserStatusCheck)) {
    std::optional<glic::CachedUserStatus> cached_user_status =
        glic::GlicUserStatusFetcher::GetCachedUserStatus(&profile);
    if (cached_user_status.has_value()) {
      is_enterprise_account_data_protected =
          cached_user_status->is_enterprise_account_data_protected;
    } else {
      // NOTE: Do not return false as a fail-closed here. CachedUserStatus is
      // only fetched when `is_managed` of
      // GlicUserStatusFetcher::UpdateUserStatus is true. Returning false means
      // gating all the non-enterprise accounts from actuation.
    }
  }
  // LINT.ThenChange(//chrome/browser/glic/glic_user_status_fetcher.cc:GlicCachedUserStatusScope)

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&profile);
  CHECK(identity_manager);
  // `account_info` is empty if the user has not signed in.
  const CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const AccountInfo extended_account_info =
      identity_manager->FindExtendedAccountInfoByAccountId(
          account_info.account_id);
  auto is_managed = extended_account_info.IsManaged();

  journal.Log(GURL(), actor::TaskId(), "IsEnterpriseAccount",
              actor::JournalDetailsBuilder()
                  .Add("is_enterprise_account_data_protected",
                       base::ToString(is_enterprise_account_data_protected))
                  .Add("is_managed", signin::TriboolToString(is_managed))
                  .Build());

  return is_enterprise_account_data_protected ||
         (is_managed == signin::Tribool::kTrue);
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
      ComputeActOnWebCapability();

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
      info.account_id == identity_manager->GetPrimaryAccountId(
                             signin::ConsentLevel::kSignin)) {
    OnPrefOrAccountChanged();
  }
}

void GlicActorPolicyChecker::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager &&
      info.account_id == identity_manager->GetPrimaryAccountId(
                             signin::ConsentLevel::kSignin)) {
    OnPrefOrAccountChanged();
  }
}

void GlicActorPolicyChecker::OnAiSubscriptionTierUpdated(
    int32_t new_subscription_tier) {
  OnPrefOrAccountChanged();
}

bool GlicActorPolicyChecker::CanActOnWeb() const {
  return can_act_on_web_ != CanActOutcome::kNo;
}

GlicActorPolicyChecker::CannotActReason
GlicActorPolicyChecker::CannotActOnWebReason() const {
  return cannot_act_on_web_reason_;
}

void GlicActorPolicyChecker::OnPrefOrAccountChanged() {
  auto old_value = can_act_on_web_;
  std::tie(can_act_on_web_, cannot_act_on_web_reason_) =
      ComputeActOnWebCapability();
  if (old_value != can_act_on_web_) {
    changed_callback_list_.Notify(CanActOnWeb());
  }
}

std::pair<GlicActorPolicyChecker::CanActOutcome,
          GlicActorPolicyChecker::CannotActReason>
GlicActorPolicyChecker::ComputeActOnWebCapability() {
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
  // `account_info` is empty if the user has not signed in.
  auto can_use_model_execution_features =
      identity_manager
          ->FindExtendedAccountInfoByAccountId(
              identity_manager
                  ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                  .account_id)
          .capabilities.can_use_model_execution_features();
  if (can_use_model_execution_features != signin::Tribool::kTrue) {
    return log_and_return(
        CanActOutcome::kNo,
        GlicActorPolicyChecker::CannotActReason::kAccountCapabilityIneligible);
  }

  bool is_likely_dogfood_client = IsLikelyDogfoodClient();
  if (is_likely_dogfood_client) {
    return log_and_return(CanActOutcome::kYes, "is likely dogfood client");
  }

  // Consumer checks.

  bool enterprise_account = IsEnterpriseAccount(*profile_, *journal_);
  bool has_management = IsBrowserManaged(*profile_);
  if (!enterprise_account && !has_management) {
    if (AccountHasChromeBenefits(*profile_, *journal_)) {
      // Only respect the consumer check if the browser is not managed.
      return log_and_return(CanActOutcome::kYes,
                            "Not managed: account has chrome benefits");
    }
    return log_and_return(CanActOutcome::kNo,
                          CannotActReason::kAccountMissingChromeBenefits);
  }

  // Chrome Enterprise policy checks.

  if (enterprise_account && !has_management) {
    // Edge (error) case: an enterprise account without management. This means
    // that policy delivery is not trustworthy (because the policy delivery over
    // a domain requires management). Fallback to the default policy pref value.
    // This should be extremely rare.
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

  // From this point on, the browser must have some level of management. Both
  // regular accounts and enterprise accounts therefore are subject to policy
  // control.

  if (ActuationEnabledForManagedUser(*profile_, *journal_)) {
    return log_and_return(CanActOutcome::kYes,
                          "Managed: actuation enabled via policy");
  }
  if (HasUrlAllowlist(*profile_)) {
    // If actuation in general is blocked by policy, but there is a non-empty
    // allow list, then we need `CanActOnWeb()` to be true so we can
    // attempt actuation up until the point where we evaluate a URL for its
    // inclusion in the allow list. If it's not explicitly allowed by the
    // list, then we perform the blocking there.
    return log_and_return(CanActOutcome::kByAllowlistOnly,
                          CannotActReason::kDisabledByPolicy);
  }
  // We reach this point only if:
  // - Account is eligible for actuation
  // - Browser has management
  //   - Actuation is disabled by policy
  //   - No URL allowlist is present
  return log_and_return(CanActOutcome::kNo, CannotActReason::kDisabledByPolicy);
}

actor::EnterprisePolicyBlockReason GlicActorPolicyChecker::Evaluate(
    const GURL& url) const {
  const policy::URLBlocklist::URLBlocklistState state =
      url_blocklist_manager_.GetURLBlocklistState(url);
  if (state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return actor::EnterprisePolicyBlockReason::kExplicitlyBlocked;
  }
  if (state == policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST) {
    return actor::EnterprisePolicyBlockReason::kExplicitlyAllowed;
  }

  // If the general policy is set to disable acting, then if the url is not in
  // the allow list, we block.
  if (can_act_on_web_ == CanActOutcome::kByAllowlistOnly) {
    return actor::EnterprisePolicyBlockReason::kExplicitlyBlocked;
  }

  return actor::EnterprisePolicyBlockReason::kNotBlocked;
}

base::CallbackListSubscription
GlicActorPolicyChecker::AddUrlListsUpdateObserverForTesting(
    base::RepeatingClosure callback) {
  return url_blocklist_manager_.AddObserver(std::move(callback));
}

}  // namespace glic
