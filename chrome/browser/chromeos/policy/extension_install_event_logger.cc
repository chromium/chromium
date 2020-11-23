// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_logger.h"

#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/disks/disk.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/pref_names.h"

namespace em = enterprise_management;

namespace policy {
ExtensionInstallEventLogger::ExtensionInstallEventLogger(
    Delegate* delegate,
    Profile* profile,
    extensions::ExtensionRegistry* registry)
    : InstallEventLoggerBase(profile),
      delegate_(delegate),
      registry_(registry),
      pref_service_(profile->GetPrefs()) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      extensions::pref_names::kInstallForceList,
      base::BindRepeating(
          &ExtensionInstallEventLogger::OnForcedExtensionsPrefChanged,
          base::Unretained(this)));
  // Try to load list now.
  OnForcedExtensionsPrefChanged();
}

ExtensionInstallEventLogger::~ExtensionInstallEventLogger() {
  if (log_collector_)
    log_collector_->OnLogout();
  pref_change_registrar_.RemoveAll();
}

void ExtensionInstallEventLogger::AddForAllExtensions(
    std::unique_ptr<em::ExtensionInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  AddForSetOfApps(pending_extensions_, std::move(event));
}

void ExtensionInstallEventLogger::Add(
    const extensions::ExtensionId& extension_id,
    bool gather_disk_space_info,
    std::unique_ptr<em::ExtensionInstallReportLogEvent> event) {
  AddEvent(extension_id, gather_disk_space_info, event);
}

void ExtensionInstallEventLogger::OnForcedExtensionsPrefChanged() {
  const base::DictionaryValue* value =
      pref_service_->GetDictionary(extensions::pref_names::kInstallForceList);
  if (!value)
    return;
  std::set<extensions::ExtensionId> current_requested;
  for (const auto& entry : *value)
    current_requested.insert(entry.first);
  const std::set<extensions::ExtensionId> previous_pending =
      pending_extensions_;

  const std::set<extensions::ExtensionId> added =
      GetDifference(current_requested, extensions_);
  const std::set<extensions::ExtensionId> removed =
      GetDifference(previous_pending, current_requested);

  if (!added.empty()) {
    AddForSetOfAppsWithDiskSpaceInfo(
        added, CreateEvent(em::ExtensionInstallReportLogEvent::POLICY_REQUEST));
  }
  if (!removed.empty()) {
    AddForSetOfApps(removed,
                    CreateEvent(em::ExtensionInstallReportLogEvent::CANCELED));
  }
  std::set<extensions::ExtensionId> current_pending = GetDifference(
      current_requested, GetDifference(extensions_, previous_pending));
  pending_extensions_ = std::move(current_pending);
  extensions_ = std::move(current_requested);

  if (!pending_extensions_.empty()) {
    UpdateCollector();
    if (initial_) {
      log_collector_->OnLogin();
      initial_ = false;
    }
    log_collector_->OnExtensionsRequested(added);
  } else {
    StopCollector();
  }
}

void ExtensionInstallEventLogger::OnExtensionInstallationFinished(
    const extensions::ExtensionId& extension_id) {
  pending_extensions_.erase(extension_id);
  UpdateCollector();
}

bool ExtensionInstallEventLogger::IsExtensionPending(
    const extensions::ExtensionId& extension_id) {
  return pending_extensions_.find(extension_id) != pending_extensions_.end();
}

void ExtensionInstallEventLogger::UpdateCollector() {
  if (pending_extensions_.empty()) {
    StopCollector();
    return;
  }
  if (!log_collector_) {
    log_collector_ = std::make_unique<ExtensionInstallEventLogCollector>(
        registry_, this, profile_);
  }
}

void ExtensionInstallEventLogger::StopCollector() {
  log_collector_.reset();
}

void ExtensionInstallEventLogger::AddForSetOfApps(
    const std::set<extensions::ExtensionId>& extensions,
    std::unique_ptr<em::ExtensionInstallReportLogEvent> event) {
  delegate_->Add(extensions, *event);
}

}  // namespace policy
