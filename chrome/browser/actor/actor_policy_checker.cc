// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_policy_checker.h"

#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/buildflags.h"
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
  // TODO(crbug.com/450525715): Wire the up enterprise policy, and
  // `BrowserManagementService::IsManaged()`.

  return profile->GetPrefs()->GetInteger(glic::prefs::kGlicActuationOnWeb) ==
         static_cast<int>(glic::prefs::GlicActuationOnWebPolicyState::kEnabled);
#endif  // BUILDFLAG(ENABLE_GLIC)
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

  // TODO(crbug.com/450525715): Depends on the shape of the Chrome API to signal
  // the HostCapability (Set vs Observable), we might need to inform the web
  // client about the capability at initialization.
}

ActorPolicyChecker::~ActorPolicyChecker() = default;

void ActorPolicyChecker::MayActOnTab(const tabs::TabInterface& tab,
                                     AggregatedJournal& journal,
                                     TaskId task_id,
                                     DecisionCallback callback) {
  if (!has_actuation_capability_) {
    journal.Log(tab.GetContents()->GetLastCommittedURL(), task_id,
                mojom::JournalTrack::kActor, "MayActOnTab",
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
    journal.Log(url, task_id, mojom::JournalTrack::kActor, "MayActOnUrl",
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
