// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_tracker.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/forced_extensions/installation_failures.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"

namespace {
// Timeout to report UMA if not all force-installed extension were loaded.
constexpr base::TimeDelta kInstallationTimeout =
    base::TimeDelta::FromMinutes(5);
}  // namespace

namespace extensions {

InstallationTracker::InstallationTracker(
    ExtensionRegistry* registry,
    Profile* profile,
    std::unique_ptr<base::OneShotTimer> timer)
    : registry_(registry),
      profile_(profile),
      pref_service_(profile->GetPrefs()),
      start_time_(base::Time::Now()),
      observer_(this),
      timer_(std::move(timer)) {
  observer_.Add(registry_);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_names::kInstallForceList,
      base::BindRepeating(&InstallationTracker::OnForcedExtensionsPrefChanged,
                          base::Unretained(this)));

  timer_->Start(
      FROM_HERE, kInstallationTimeout,
      base::BindRepeating(&InstallationTracker::ReportResults,
                          base::Unretained(this), false /* succeeded */));

  // Try to load list now.
  OnForcedExtensionsPrefChanged();
}

InstallationTracker::~InstallationTracker() = default;

void InstallationTracker::OnForcedExtensionsPrefChanged() {
  // Load forced extensions list only once.
  if (!forced_extensions_.empty())
    return;

  const base::DictionaryValue* value =
      pref_service_->GetDictionary(pref_names::kInstallForceList);
  if (!value || value->empty())
    return;

  for (const auto& entry : *value) {
    forced_extensions_.insert(entry.first);
    if (!registry_->enabled_extensions().Contains(entry.first))
      pending_forced_extensions_.insert(entry.first);
  }
  if (pending_forced_extensions_.empty())
    ReportResults(true /* succeeded */);
}

void InstallationTracker::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (pending_forced_extensions_.erase(extension->id()) &&
      pending_forced_extensions_.empty()) {
    ReportResults(true /* succeeded */);
  }
}

void InstallationTracker::ReportResults(bool succeeded) {
  DCHECK(!reported_);
  // Report only if there was non-empty list of force-installed extensions.
  if (!forced_extensions_.empty()) {
    if (succeeded) {
      UMA_HISTOGRAM_LONG_TIMES("Extensions.ForceInstalledLoadTime",
                               base::Time::Now() - start_time_);
    } else {
      size_t enabled_missing_count = pending_forced_extensions_.size();
      auto installed_extensions = registry_->GenerateInstalledExtensionsSet();
      for (const auto& entry : *installed_extensions)
        pending_forced_extensions_.erase(entry->id());
      size_t installed_missing_count = pending_forced_extensions_.size();

      UMA_HISTOGRAM_COUNTS_100("Extensions.ForceInstalledTimedOutCount",
                               enabled_missing_count);
      UMA_HISTOGRAM_COUNTS_100(
          "Extensions.ForceInstalledTimedOutAndNotInstalledCount",
          installed_missing_count);
      for (const auto& extension_id : pending_forced_extensions_) {
        InstallationFailures::Reason reason =
            InstallationFailures::Get(profile_, extension_id);
        UMA_HISTOGRAM_ENUMERATION("Extensions.ForceInstalledFailureReason",
                                  reason);
      }
    }
  }
  reported_ = true;
  InstallationFailures::Clear(profile_);
  observer_.RemoveAll();
  pref_change_registrar_.RemoveAll();
  timer_->Stop();
}

}  //  namespace extensions
