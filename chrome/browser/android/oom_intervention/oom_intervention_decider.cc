// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/metrics/metrics_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

const char kOomInterventionDecider[] = "oom_intervention.decider";

// Deprecated: Replaced with `kBlocklist`.
// TODO(crbug.com/40744119): Remove this after M92 once existing
// clients have migrated to the new pref.
const char kBlacklist[] = "oom_intervention.blacklist";
// Pref path for blocklist. If a hostname is in the blocklist we never trigger
// intervention on the host.
const char kBlocklist[] = "oom_intervention.blocklist";
// Pref path for declined host list. If a hostname is in the declined host list
// we don't trigger intervention until a OOM crash happens on the host.
const char kDeclinedHostList[] = "oom_intervention.declined_host_list";
// Pref path for OOM detected host list. When an OOM crash is observed on
// a host the hostname is added to the list.
const char kOomDetectedHostList[] = "oom_intervention.oom_detected_host_list";

class DelegateImpl : public OomInterventionDecider::Delegate {
 public:
  bool WasLastShutdownClean() override {
    if (!g_browser_process || !g_browser_process->metrics_service())
      return true;
    return g_browser_process->metrics_service()->WasLastShutdownClean();
  }
};

}  // namespace

const size_t OomInterventionDecider::kMaxListSize = 10;
const size_t OomInterventionDecider::kMaxBlocklistSize = 6;

// static
void OomInterventionDecider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kBlocklist);
  registry->RegisterListPref(kDeclinedHostList);
  registry->RegisterListPref(kOomDetectedHostList);

  // Continue to register the old preference to migrate its value.
  registry->RegisterListPref(kBlacklist);
}

// static
OomInterventionDecider* OomInterventionDecider::GetForBrowserContext(
    content::BrowserContext* context) {
  // The OomIntervetnionDecider is disabled in incognito mode because it is
  // written in such a way that hostnames would be persisted in preferences on
  // disk which is not acceptable for incognito mode.
  if (context->IsOffTheRecord())
    return nullptr;

  if (!context->GetUserData(kOomInterventionDecider)) {
    PrefService* prefs = Profile::FromBrowserContext(context)->GetPrefs();
    context->SetUserData(kOomInterventionDecider,
                         base::WrapUnique(new OomInterventionDecider(
                             std::make_unique<DelegateImpl>(), prefs)));
  }
  return static_cast<OomInterventionDecider*>(
      context->GetUserData(kOomInterventionDecider));
}

OomInterventionDecider::OomInterventionDecider(
    std::unique_ptr<Delegate> delegate,
    PrefService* prefs)
    : delegate_(std::move(delegate)), prefs_(prefs) {
  DCHECK(delegate_);

  PrefService::PrefInitializationStatus pref_status =
      prefs_->GetInitializationStatus();
  if (pref_status == PrefService::INITIALIZATION_STATUS_WAITING) {
    prefs_->AddPrefInitObserver(base::BindOnce(
        &OomInterventionDecider::OnPrefInitialized, base::Unretained(this)));
  } else {
    OnPrefInitialized(pref_status ==
                      PrefService::INITIALIZATION_STATUS_SUCCESS);
  }
}

OomInterventionDecider::~OomInterventionDecider() = default;

bool OomInterventionDecider::CanTriggerIntervention(
    const std::string& host) const {
  if (IsOptedOut(host))
    return false;

  // Check whether OOM was observed before checking declined host list in favor
  // of triggering intervention after OOM.
  if (IsInList(kOomDetectedHostList, host))
    return true;

  if (IsInList(kDeclinedHostList, host))
    return false;

  return true;
}

void OomInterventionDecider::OnInterventionDeclined(const std::string& host) {
  if (IsOptedOut(host))
    return;

  if (IsInList(kDeclinedHostList, host)) {
    AddToList(kBlocklist, host);
  } else {
    AddToList(kDeclinedHostList, host);
  }
}

void OomInterventionDecider::OnOomDetected(const std::string& host) {
  if (IsOptedOut(host))
    return;
  AddToList(kOomDetectedHostList, host);
}

void OomInterventionDecider::ClearData() {
  prefs_->ClearPref(kBlocklist);
  prefs_->ClearPref(kDeclinedHostList);
  prefs_->ClearPref(kOomDetectedHostList);
}

void OomInterventionDecider::OnPrefInitialized(bool success) {
  if (!success)
    return;

  // Migrate `kBlacklist` to `kBlocklist`.
  const base::Value::List& old_pref_value = prefs_->GetList(kBlacklist);
  if (!old_pref_value.empty()) {
    prefs_->SetList(kBlocklist, old_pref_value.Clone());
    prefs_->SetList(kBlacklist, base::Value::List());
  }

  if (delegate_->WasLastShutdownClean())
    return;

  const base::Value::List& declined_list = prefs_->GetList(kDeclinedHostList);
  if (!declined_list.empty()) {
    const std::string& last_declined = declined_list.back().GetString();
    if (!IsInList(kBlocklist, last_declined))
      AddToList(kOomDetectedHostList, last_declined);
  }
}

bool OomInterventionDecider::IsOptedOut(const std::string& host) const {
  if (prefs_->GetList(kBlocklist).size() >= kMaxBlocklistSize)
    return true;

  return IsInList(kBlocklist, host);
}

bool OomInterventionDecider::IsInList(const char* list_name,
                                      const std::string& host) const {
  for (const auto& value : prefs_->GetList(list_name)) {
    if (value.GetString() == host)
      return true;
  }
  return false;
}

void OomInterventionDecider::AddToList(const char* list_name,
                                       const std::string& host) {
  if (IsInList(list_name, host))
    return;
  ScopedListPrefUpdate update(prefs_, list_name);
  base::Value::List& update_list = update.Get();
  update_list.Append(host);
  if (update_list.size() > kMaxListSize)
    update_list.erase(update_list.begin());

  // Save the list immediately because we typically modify lists under high
  // memory pressure, in which the browser process can be killed by the OS
  // soon.
  prefs_->CommitPendingWrite();
}
