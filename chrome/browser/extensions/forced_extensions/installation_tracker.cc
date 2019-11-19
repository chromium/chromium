// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_tracker.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"

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
  const base::DictionaryValue* value =
      pref_service_->GetDictionary(pref_names::kInstallForceList);
  if (!value)
    return;

  std::vector<ExtensionId> extensions_to_remove;
  for (const auto& extension_id : forced_extensions_) {
    if (value->FindKey(extension_id) == nullptr)
      extensions_to_remove.push_back(extension_id);
  }

  for (const auto& extension_id : extensions_to_remove) {
    forced_extensions_.erase(extension_id);
    pending_forced_extensions_.erase(extension_id);
  }

  // Report if all remaining extensions were removed from policy.
  if (loaded_ && pending_forced_extensions_.empty())
    ReportResults(true /* succeeded */);

  // Load forced extensions list only once.
  if (value->empty() || loaded_) {
    return;
  }

  loaded_ = true;

  for (const auto& entry : *value) {
    forced_extensions_.insert(entry.first);
    if (!registry_->enabled_extensions().Contains(entry.first))
      pending_forced_extensions_.insert(entry.first);
  }
  if (pending_forced_extensions_.empty())
    ReportResults(true /* succeeded */);
}

void InstallationTracker::OnShutdown(ExtensionRegistry*) {
  observer_.RemoveAll();
  pref_change_registrar_.RemoveAll();
  timer_->Stop();
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
    UMA_HISTOGRAM_COUNTS_100("Extensions.ForceInstalledTotalCandidateCount",
                             forced_extensions_.size());
    if (succeeded) {
      UMA_HISTOGRAM_LONG_TIMES("Extensions.ForceInstalledLoadTime",
                               base::Time::Now() - start_time_);
      // TODO(burunduk): Remove VLOGs after resolving crbug/917700 and
      // crbug/904600.
      VLOG(2) << "All forced extensions seems to be installed";
    } else {
      InstallationReporter* installation_reporter =
          InstallationReporter::Get(profile_);
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
      VLOG(2) << "Failed to install " << installed_missing_count
              << " forced extensions.";
      for (const auto& extension_id : pending_forced_extensions_) {
        InstallationReporter::InstallationData installation =
            installation_reporter->Get(extension_id);
        UMA_HISTOGRAM_ENUMERATION(
            "Extensions.ForceInstalledFailureCacheStatus",
            installation.downloading_cache_status.value_or(
                ExtensionDownloaderDelegate::CacheStatus::CACHE_UNKNOWN));
        if (!installation.failure_reason && installation.install_stage) {
          installation.failure_reason =
              InstallationReporter::FailureReason::IN_PROGRESS;
          InstallationReporter::Stage install_stage =
              installation.install_stage.value();
          UMA_HISTOGRAM_ENUMERATION("Extensions.ForceInstalledStage",
                                    install_stage);
          if (install_stage == InstallationReporter::Stage::DOWNLOADING) {
            DCHECK(installation.downloading_stage);
            ExtensionDownloaderDelegate::Stage downloading_stage =
                installation.downloading_stage.value();
            UMA_HISTOGRAM_ENUMERATION(
                "Extensions.ForceInstalledDownloadingStage", downloading_stage);
          }
        }
        InstallationReporter::FailureReason failure_reason =
            installation.failure_reason.value_or(
                InstallationReporter::FailureReason::UNKNOWN);
        UMA_HISTOGRAM_ENUMERATION("Extensions.ForceInstalledFailureReason",
                                  failure_reason);
        VLOG(2) << "Forced extension " << extension_id
                << " failed to install with data="
                << InstallationReporter::GetFormattedInstallationData(
                       installation);
        if (installation.install_error_detail) {
          CrxInstallErrorDetail detail =
              installation.install_error_detail.value();
          UMA_HISTOGRAM_ENUMERATION(
              "Extensions.ForceInstalledFailureCrxInstallError", detail);
        }
      }
    }
  }
  reported_ = true;
  InstallationReporter::Get(profile_)->Clear();
  observer_.RemoveAll();
  pref_change_registrar_.RemoveAll();
  timer_->Stop();
}

}  //  namespace extensions
