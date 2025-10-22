// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_policy_checker.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/version.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

bool HasActuationCapability(Profile* profile) {
  CHECK(profile);
  CHECK(profile->GetPrefs());

#if !BUILDFLAG(ENABLE_GLIC)
  return true;
#else

  auto* management_service_factory =
      policy::ManagementServiceFactory::GetInstance();
  auto* browser_management_service =
      management_service_factory->GetForProfile(profile);

  const bool is_managed =
      browser_management_service && browser_management_service->IsManaged();
  const bool is_managed_trial = base::FeatureList::IsEnabled(
      features::kGlicActOnWebCapabilityForManagedTrials);
  // M143-M145: Managed clients do not have the capability. The policy cannot be
  // enabled the capability for managed clients.
  if (!is_managed || is_managed_trial) {
    // Non-managed clients or trial participants have the capability by
    // default.
    return profile->GetPrefs()->GetInteger(glic::prefs::kGlicActuationOnWeb) !=
           static_cast<int>(
               glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
  }

  const base::Version version = version_info::GetVersion();
  if (version.IsValid() && version < base::Version("145")) {
    return false;
  }

  // M145-M147: Managed clients defaults to no capability, but can be enabled by
  // the policy.
  if (version.IsValid() && version < base::Version("147")) {
    return profile->GetPrefs()->GetInteger(glic::prefs::kGlicActuationOnWeb) ==
           static_cast<int>(
               glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
  }

  // M147: Managed clients have the capability by default. The policy can be
  // used to disable the capability for managed clients.
  // Invalid version also defaults to enabled.
  return profile->GetPrefs()->GetInteger(glic::prefs::kGlicActuationOnWeb) !=
         static_cast<int>(
             glic::prefs::GlicActuationOnWebPolicyState::kDisabled);
#endif  // !BUILDFLAG(ENABLE_GLIC)
}

}  // namespace

ActorPolicyChecker::ActorPolicyChecker(ActorKeyedService& service)
    : service_(service) {
  InitActionBlocklist(service.GetProfile());

  has_actuation_capability_ = HasActuationCapability(service.GetProfile());

  pref_change_registrar_.Init(service.GetProfile()->GetPrefs());
  pref_change_registrar_.Add(
      glic::prefs::kGlicActuationOnWeb,
      base::BindRepeating(&ActorPolicyChecker::OnPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug.com/450525715, crbug.com/452416162): The web client needs to be
  // informed of the initial capability value.
}

ActorPolicyChecker::~ActorPolicyChecker() = default;

void ActorPolicyChecker::MayActOnTab(const tabs::TabInterface& tab,
                                     AggregatedJournal& journal,
                                     TaskId task_id,
                                     DecisionCallback callback) {
  if (!has_actuation_capability_) {
    journal.Log(tab.GetContents()->GetLastCommittedURL(), task_id,
                "MayActOnTab",
                JournalDetailsBuilder()
                    .AddError("Actuation capability disabled")
                    .Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*decision=*/false));
    return;
  }
  ::actor::MayActOnTab(tab, journal, task_id, std::move(callback));
}

void ActorPolicyChecker::MayActOnUrl(const GURL& url,
                                     bool allow_insecure_http,
                                     Profile* profile,
                                     AggregatedJournal& journal,
                                     TaskId task_id,
                                     DecisionCallback callback) {
  if (!has_actuation_capability_) {
    journal.Log(url, task_id, "MayActOnUrl",
                JournalDetailsBuilder()
                    .AddError("Actuation capability disabled")
                    .Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*decision=*/false));
    return;
  }
  ::actor::MayActOnUrl(url, allow_insecure_http, profile, journal, task_id,
                       std::move(callback));
}

void ActorPolicyChecker::OnPrefChanged() {
  has_actuation_capability_ = HasActuationCapability(service_->GetProfile());
  service_->OnActuationCapabilityChanged(has_actuation_capability_);
}

}  // namespace actor
