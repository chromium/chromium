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
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

#if BUILDFLAG(ENABLE_GLIC)
std::string GlicActorEnterprisePrefDefaultToString(
    features::GlicActorEnterprisePrefDefault value) {
  switch (value) {
    case features::GlicActorEnterprisePrefDefault::kEnabledByDefault:
      return "enabled_by_default";
    case features::GlicActorEnterprisePrefDefault::kDisabledByDefault:
      return "disabled_by_default";
    case features::GlicActorEnterprisePrefDefault::kForcedDisabled:
      return "forced_disabled";
  }
}

std::string GlicActuationOnWebPrefToString(int value) {
  if (value == base::to_underlying(
                   glic::prefs::GlicActuationOnWebPolicyState::kEnabled)) {
    return "kEnabled";
  } else if (value ==
             base::to_underlying(
                 glic::prefs::GlicActuationOnWebPolicyState::kDisabled)) {
    return "kDisabled";
  }
  NOTREACHED();
}

bool IsLikelyDogfoodClient() {
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  return variations_service && variations_service->IsLikelyDogfoodClient();
}
#endif  // BUILDFLAG(ENABLE_GLIC)

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
  const features::GlicActorEnterprisePrefDefault default_pref =
      features::kGlicActorEnterprisePrefDefault.Get();
  const int capability_pref =
      profile->GetPrefs()->GetInteger(glic::prefs::kGlicActuationOnWeb);
  bool is_likely_dogfood_client = IsLikelyDogfoodClient();

  VLOG(1) << "Is browser managed: " << is_managed;
  VLOG(1) << "kGlicActorEnterprisePrefDefault value: "
          << GlicActorEnterprisePrefDefaultToString(default_pref);
  VLOG(1) << "kGlicActuationOnWeb is_managed: "
          << profile->GetPrefs()->IsManagedPreference(
                 glic::prefs::kGlicActuationOnWeb)
          << " value: " << GlicActuationOnWebPrefToString(capability_pref);
  VLOG(1) << "is_likely_dogfood_client: " << is_likely_dogfood_client;

  if (!is_managed || is_likely_dogfood_client) {
    return true;
  }

  if (default_pref ==
      features::GlicActorEnterprisePrefDefault::kForcedDisabled) {
    return false;
  }

  return capability_pref ==
         base::to_underlying(
             glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
#endif  // !BUILDFLAG(ENABLE_GLIC)
}

}  // namespace

ActorPolicyChecker::ActorPolicyChecker(ActorKeyedService& service)
    : service_(service) {
  InitActionBlocklist(service.GetProfile());

  can_act_on_web_ = HasActuationCapability(service.GetProfile());

  pref_change_registrar_.Init(service.GetProfile()->GetPrefs());
  pref_change_registrar_.Add(
      glic::prefs::kGlicActuationOnWeb,
      base::BindRepeating(&ActorPolicyChecker::OnPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

ActorPolicyChecker::~ActorPolicyChecker() = default;

void ActorPolicyChecker::MayActOnTab(
    const tabs::TabInterface& tab,
    AggregatedJournal& journal,
    TaskId task_id,
    const absl::flat_hash_set<url::Origin>& allowed_origins,
    DecisionCallback callback) {
  if (!can_act_on_web()) {
    journal.Log(tab.GetContents()->GetLastCommittedURL(), task_id,
                "MayActOnTab",
                JournalDetailsBuilder()
                    .AddError("Actuation capability disabled")
                    .Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*decision=*/false));
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
                                     DecisionCallback callback) {
  // TODO(http://crbug.com/455645486): This may be turned into a CHECK.
  if (!can_act_on_web()) {
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
  can_act_on_web_ = HasActuationCapability(service_->GetProfile());
  service_->OnActOnWebCapabilityChanged(can_act_on_web_);
}

}  // namespace actor
