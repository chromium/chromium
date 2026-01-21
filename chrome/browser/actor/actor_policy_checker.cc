// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_policy_checker.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/to_string.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service_factory.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_GLIC)
#include <ostream>
#include <string_view>
#include <variant>

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/glic_user_status_fetcher.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

#if BUILDFLAG(ENABLE_GLIC)
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

namespace glic::prefs {
std::ostream& operator<<(std::ostream& os,
                         GlicActuationOnWebPolicyState value) {
  switch (value) {
    case GlicActuationOnWebPolicyState::kEnabled:
      return os << "kEnabled";
    case GlicActuationOnWebPolicyState::kDisabled:
      return os << "kDisabled";
  }
}
}  // namespace glic::prefs

namespace actor {
std::ostream& operator<<(std::ostream& os,
                         ActorPolicyChecker::CanActOutcome value) {
  switch (value) {
    case ActorPolicyChecker::CanActOutcome::kYes:
      return os << "kYes";
    case ActorPolicyChecker::CanActOutcome::kNo:
      return os << "kNo";
    case ActorPolicyChecker::CanActOutcome::kByAllowlistOnly:
      return os << "kByAllowlistOnly";
  }
}

std::ostream& operator<<(std::ostream& os,
                         ActorPolicyChecker::CannotActReason value) {
  switch (value) {
    case ActorPolicyChecker::CannotActReason::kNone:
      return os << "kNone";
    case ActorPolicyChecker::CannotActReason::kAccountCapabilityIneligible:
      return os << "kAccountCapabilityIneligible";
    case ActorPolicyChecker::CannotActReason::kAccountMissingChromeBenefits:
      return os << "kAccountMissingChromeBenefits";
    case ActorPolicyChecker::CannotActReason::kManagedOrDataProtected:
      return os << "kManagedOrDataProtected";
  }
}

std::ostream& operator<<(
    std::ostream& os,
    std::variant<ActorPolicyChecker::CannotActReason, std::string_view>
        value) {
  std::visit([&os](auto&& arg) { os << arg; }, value);
  return os;
}

}  // namespace actor
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace actor {

namespace {

#if BUILDFLAG(ENABLE_GLIC)

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
                                    AggregatedJournal& journal) {
  features::GlicActorEnterprisePrefDefault default_pref =
      features::kGlicActorEnterprisePrefDefault.Get();
  auto* pref_service = profile.GetPrefs();
  CHECK(pref_service);
  auto capability_pref =
      static_cast<glic::prefs::GlicActuationOnWebPolicyState>(
          pref_service->GetInteger(glic::prefs::kGlicActuationOnWeb));
  journal.Log(GURL(), TaskId(), "ActuationEnabledForManagedUser",
              JournalDetailsBuilder()
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
  const base::Value::List& allowlist =
      pref_service->GetList(allowlist_pref_path);
  return !allowlist.empty();
}

// Returns true if !is_enterprise_account_data_protected &&
// !AccountInfo::IsManaged().
bool IsAccountEligibleForActuation(Profile& profile,
                                   AggregatedJournal& journal) {
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

  journal.Log(GURL(), TaskId(), "IsAccountEligibleForActuation",
              JournalDetailsBuilder()
                  .Add("is_enterprise_account_data_protected",
                       base::ToString(is_enterprise_account_data_protected))
                  .Add("is_managed", signin::TriboolToString(is_managed))
                  .Build());

  return !is_enterprise_account_data_protected &&
         (is_managed == signin::Tribool::kFalse);
}

// TODO(crbug.com/471065012): This is a consumer check so it should be moved to
// the overall actuation account access check. Placed here for a quick fix.
bool AccountHasChromeBenefits(Profile& profile, AggregatedJournal& journal) {
  subscription_eligibility::SubscriptionEligibilityService*
      subscription_service = subscription_eligibility::
          SubscriptionEligibilityServiceFactory::GetForProfile(&profile);
  CHECK(subscription_service);
  const base::flat_set<int32_t>& eligible_tiers =
      ActorPolicyChecker::GetActorEligibleTiers();
  int32_t subscription_tier = subscription_service->GetAiSubscriptionTier();
  journal.Log(
      GURL(), TaskId(), "AccountHasChromeBenefits",
      JournalDetailsBuilder()
          .Add("subscription_tier", subscription_tier)
          .Add("eligible_tiers", features::kGlicActorEligibleTiers.Get())
          .Build());
  return eligible_tiers.contains(subscription_tier);
}

#endif  // BUILDFLAG(ENABLE_GLIC)
}  // namespace

ActorPolicyChecker::ActorPolicyChecker(ActorKeyedService& service)
    : service_(service),
#if BUILDFLAG(ENABLE_GLIC)
      url_blocklist_manager_(service.GetProfile()->GetPrefs(),
                             glic::prefs::kGlicActuationOnWebBlockedForURLs,
                             glic::prefs::kGlicActuationOnWebAllowedForURLs),
#endif  // BUILDFLAG(ENABLE_GLIC)
      journal_(service.GetJournal().GetSafeRef()) {
  InitActionBlocklist(service.GetProfile());

#if BUILDFLAG(ENABLE_GLIC)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(service.GetProfile());
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
  subscription_eligibility::SubscriptionEligibilityService*
      subscription_service =
          subscription_eligibility::SubscriptionEligibilityServiceFactory::
              GetForProfile(service.GetProfile());
  if (subscription_service) {
    subscription_eligibility_service_observation_.Observe(subscription_service);
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  std::tie(can_act_on_web_, cannot_act_on_web_reason_) =
      ComputeActOnWebCapability();

  pref_change_registrar_.Init(service.GetProfile()->GetPrefs());
#if BUILDFLAG(ENABLE_GLIC)
  // Listens to policy changes.
  pref_change_registrar_.Add(
      glic::prefs::kGlicActuationOnWeb,
      base::BindRepeating(&ActorPolicyChecker::OnPrefOrAccountChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  url_blocklist_subscription_ = url_blocklist_manager_.AddObserver(
      base::BindRepeating(&ActorPolicyChecker::OnPrefOrAccountChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  // Listens to user status changes.
  pref_change_registrar_.Add(
      glic::prefs::kGlicUserStatus,
      base::BindRepeating(&ActorPolicyChecker::OnPrefOrAccountChanged,
                          weak_ptr_factory_.GetWeakPtr()));
#endif  // BUILDFLAG(ENABLE_GLIC)
}

ActorPolicyChecker::~ActorPolicyChecker() = default;

// static
const base::flat_set<int32_t>& ActorPolicyChecker::GetActorEligibleTiers() {
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

void ActorPolicyChecker::OnPrimaryAccountChanged(
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

void ActorPolicyChecker::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(service_->GetProfile());
  if (identity_manager &&
      info.account_id == identity_manager->GetPrimaryAccountId(
                             signin::ConsentLevel::kSignin)) {
    OnPrefOrAccountChanged();
  }
}

void ActorPolicyChecker::OnExtendedAccountInfoRemoved(const AccountInfo& info) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(service_->GetProfile());
  if (identity_manager &&
      info.account_id == identity_manager->GetPrimaryAccountId(
                             signin::ConsentLevel::kSignin)) {
    OnPrefOrAccountChanged();
  }
}

void ActorPolicyChecker::OnAiSubscriptionTierUpdated(
    int32_t new_subscription_tier) {
  OnPrefOrAccountChanged();
}

void ActorPolicyChecker::MayActOnTab(const tabs::TabInterface& tab,
                                     AggregatedJournal& journal,
                                     TaskId task_id,
                                     const OriginChecker& origin_checker,
                                     DecisionCallbackWithReason callback) {
  if (!CanActOnWeb()) {
    journal.Log(tab.GetContents()->GetLastCommittedURL(), task_id,
                "MayActOnTab",
                JournalDetailsBuilder()
                    .AddError("Actuation capability disabled")
                    .Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  MayActOnUrlBlockReason::kActuactionDisabled));
    return;
  }
  ::actor::MayActOnTab(
      tab, journal, task_id, origin_checker,
      [this](const GURL& url) { return EvaluateEnterprisePolicyForUrl(url); },
      std::move(callback));
}

void ActorPolicyChecker::MayActOnUrl(const GURL& url,
                                     bool allow_insecure_http,
                                     Profile* profile,
                                     AggregatedJournal& journal,
                                     TaskId task_id,
                                     DecisionCallbackWithReason callback) {
  // TODO(http://crbug.com/455645486): This may be turned into a CHECK.
  if (!CanActOnWeb()) {
    journal.Log(url, task_id, "MayActOnUrl",
                JournalDetailsBuilder()
                    .AddError("Actuation capability disabled")
                    .Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  MayActOnUrlBlockReason::kActuactionDisabled));
    return;
  }
  ::actor::MayActOnUrl(
      url, allow_insecure_http, profile, journal, task_id,
      [this](const GURL& url) { return EvaluateEnterprisePolicyForUrl(url); },
      std::move(callback));
}

bool ActorPolicyChecker::CanActOnWeb() const {
  return can_act_on_web_ != CanActOutcome::kNo;
}

ActorPolicyChecker::CannotActReason ActorPolicyChecker::CannotActOnWebReason()
    const {
  return cannot_act_on_web_reason_;
}

void ActorPolicyChecker::OnPrefOrAccountChanged() {
  auto old_value = can_act_on_web_;
  std::tie(can_act_on_web_, cannot_act_on_web_reason_) =
      ComputeActOnWebCapability();
  if (old_value != can_act_on_web_) {
    service_->OnActOnWebCapabilityChanged(CanActOnWeb());
  }
}

std::pair<ActorPolicyChecker::CanActOutcome,
          ActorPolicyChecker::CannotActReason>
ActorPolicyChecker::ComputeActOnWebCapability() {
#if !BUILDFLAG(ENABLE_GLIC)
  return {CanActOutcome::kYes, CannotActReason::kNone};
#else
  auto log_and_return =
      [&](CanActOutcome outcome,
          std::variant<CannotActReason, std::string_view> reason) {
        CHECK(outcome == CanActOutcome::kYes ||
              std::holds_alternative<CannotActReason>(reason));
        journal_->Log(GURL(), TaskId(),
                      "ActorPolicyChecker::ComputeActOnWebCapability",
                      JournalDetailsBuilder()
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
  auto* profile = service_->GetProfile();
  CHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
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
        ActorPolicyChecker::CannotActReason::kAccountCapabilityIneligible);
  }

  bool is_likely_dogfood_client = IsLikelyDogfoodClient();
  if (is_likely_dogfood_client) {
    return log_and_return(CanActOutcome::kYes, "is likely dogfood client");
  }

  bool account_eligible_for_actuation =
      IsAccountEligibleForActuation(*profile, *journal_);
  if (account_eligible_for_actuation_for_testing_) [[unlikely]] {
    account_eligible_for_actuation = true;
  }
  if (!account_eligible_for_actuation) {
    return log_and_return(CanActOutcome::kNo,
                          CannotActReason::kManagedOrDataProtected);
  }

  if (!IsBrowserManaged(*profile)) {
    if (AccountHasChromeBenefits(*profile, *journal_)) {
      // Only respect the consumer check if the browser is not managed.
      return log_and_return(CanActOutcome::kYes,
                            "Not managed: account has chrome benefits");
    }
    return log_and_return(CanActOutcome::kNo,
                          CannotActReason::kAccountMissingChromeBenefits);
  }

  if (ActuationEnabledForManagedUser(*profile, *journal_)) {
    return log_and_return(CanActOutcome::kYes,
                          "Managed: actuation enabled via policy");
  }
  if (HasUrlAllowlist(*profile)) {
    // If actuation in general is blocked by policy, but there is a non-empty
    // allow list, then we need `CanActOnWeb()` to be true so we can
    // attempt actuation up until the point where we evaluate a URL for its
    // inclusion in the allow list. If it's not explicitly allowed by the
    // list, then we perform the blocking there.
    return log_and_return(CanActOutcome::kByAllowlistOnly,
                          CannotActReason::kManagedOrDataProtected);
  }
  // We reach this point only if:
  // - Account is eligible for actuation
  // - Browser has management
  //   - Actuation is disabled by policy
  //   - No URL allowlist is present
  return log_and_return(CanActOutcome::kNo,
                        CannotActReason::kManagedOrDataProtected);
#endif  // !BUILDFLAG(ENABLE_GLIC)
}

EnterprisePolicyBlockReason ActorPolicyChecker::EvaluateEnterprisePolicyForUrl(
    const GURL& url) const {
#if !BUILDFLAG(ENABLE_GLIC)
  return EnterprisePolicyBlockReason::kNotBlocked;
#else
  const policy::URLBlocklist::URLBlocklistState state =
      url_blocklist_manager_.GetURLBlocklistState(url);
  if (state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return EnterprisePolicyBlockReason::kExplicitlyBlocked;
  }
  if (state == policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST) {
    return EnterprisePolicyBlockReason::kExplicitlyAllowed;
  }

  // If the general policy is set to disable acting, then if the url is not in
  // the allow list, we block.
  if (can_act_on_web_ == CanActOutcome::kByAllowlistOnly) {
    return EnterprisePolicyBlockReason::kExplicitlyBlocked;
  }

  return EnterprisePolicyBlockReason::kNotBlocked;
#endif  // !BUILDFLAG(ENABLE_GLIC)
}

#if BUILDFLAG(ENABLE_GLIC)
base::CallbackListSubscription
ActorPolicyChecker::AddUrlListsUpdateObserverForTesting(
    base::RepeatingClosure callback) {
  return url_blocklist_manager_.AddObserver(std::move(callback));
}
#endif  // BUILDFLAG(ENABLE_GLIC)

}  // namespace actor
