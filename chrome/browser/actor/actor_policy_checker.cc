// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_policy_checker.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
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

  if (is_enterprise_account_data_protected) {
    return false;
  }
  return is_managed == signin::Tribool::kFalse;
}

#endif  // BUILDFLAG(ENABLE_GLIC)
}  // namespace

ActorPolicyChecker::ActorPolicyChecker(ActorKeyedService& service)
    : service_(service), journal_(service.GetJournal().GetSafeRef()) {
  InitActionBlocklist(service.GetProfile());

#if BUILDFLAG(ENABLE_GLIC)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(service.GetProfile());
  if (identity_manager) {
    identity_manager_observation_.Observe(identity_manager);
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  can_act_on_web_ = ComputeActOnWebCapability();

  pref_change_registrar_.Init(service.GetProfile()->GetPrefs());
#if BUILDFLAG(ENABLE_GLIC)
  // Listens to policy changes.
  pref_change_registrar_.Add(
      glic::prefs::kGlicActuationOnWeb,
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

void ActorPolicyChecker::MayActOnTab(
    const tabs::TabInterface& tab,
    AggregatedJournal& journal,
    TaskId task_id,
    const absl::flat_hash_set<url::Origin>& allowed_origins,
    DecisionCallbackWithReason callback) {
  if (!can_act_on_web()) {
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
  ::actor::MayActOnTab(tab, journal, task_id, allowed_origins,
                       std::move(callback));
}

void ActorPolicyChecker::MayActOnUrl(const GURL& url,
                                     bool allow_insecure_http,
                                     Profile* profile,
                                     AggregatedJournal& journal,
                                     TaskId task_id,
                                     DecisionCallbackWithReason callback) {
  // TODO(http://crbug.com/455645486): This may be turned into a CHECK.
  if (!can_act_on_web()) {
    journal.Log(url, task_id, "MayActOnUrl",
                JournalDetailsBuilder()
                    .AddError("Actuation capability disabled")
                    .Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  MayActOnUrlBlockReason::kActuactionDisabled));
    return;
  }
  ::actor::MayActOnUrl(url, allow_insecure_http, profile, journal, task_id,
                       std::move(callback));
}

void ActorPolicyChecker::OnPrefOrAccountChanged() {
  auto old_value = can_act_on_web_;
  can_act_on_web_ = ComputeActOnWebCapability();
  if (old_value != can_act_on_web_) {
    service_->OnActOnWebCapabilityChanged(can_act_on_web_);
  }
}

bool ActorPolicyChecker::ComputeActOnWebCapability() {
#if !BUILDFLAG(ENABLE_GLIC)
  return true;
#else
  bool policy_exemption = features::kGlicActorPolicyControlExemption.Get();
  bool is_likely_dogfood_client = IsLikelyDogfoodClient();
  auto* profile = service_->GetProfile();
  CHECK(profile);
  bool is_browser_managed = IsBrowserManaged(*service_->GetProfile());
  bool actuation_enabled_for_managed_user = false;
  if (is_browser_managed) {
    actuation_enabled_for_managed_user =
        ActuationEnabledForManagedUser(*profile, *journal_);
  }
  bool account_eligible_for_actuation =
      IsAccountEligibleForActuation(*profile, *journal_);
  journal_->Log(
      GURL(), TaskId(), "ActorPolicyChecker::ComputeActOnWebCapability",
      JournalDetailsBuilder()
          .Add("policy_exemption", base::ToString(policy_exemption))
          .Add("is_likely_dogfood_client",
               base::ToString(is_likely_dogfood_client))
          .Add("is_browser_managed", base::ToString(is_browser_managed))

          .Add("account_eligible_for_actuation",
               base::ToString(account_eligible_for_actuation))
          .Add("actuation_enabled_for_managed_user",
               base::ToString(actuation_enabled_for_managed_user))
          .Build());

  if (is_likely_dogfood_client || policy_exemption) {
    return true;
  }
  if (account_eligible_for_actuation_for_testing_) [[unlikely]] {
    account_eligible_for_actuation = true;
  }
  return (!is_browser_managed || actuation_enabled_for_managed_user) &&
         account_eligible_for_actuation;
#endif  // !BUILDFLAG(ENABLE_GLIC)
}

}  // namespace actor
