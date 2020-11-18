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
namespace {

// Return all elements that are members of |first| but not |second|.
std::set<extensions::ExtensionId> GetDifference(
    const std::set<extensions::ExtensionId>& first,
    const std::set<extensions::ExtensionId>& second) {
  std::set<extensions::ExtensionId> difference;
  std::set_difference(first.begin(), first.end(), second.begin(), second.end(),
                      std::inserter(difference, difference.end()));
  return difference;
}

std::unique_ptr<em::ExtensionInstallReportLogEvent> AddDiskSpaceInfoToEvent(
    std::unique_ptr<em::ExtensionInstallReportLogEvent> event,
    const base::FilePath& stateful_path) {
  const int64_t stateful_total =
      base::SysInfo::AmountOfTotalDiskSpace(stateful_path);
  if (stateful_total >= 0)
    event->set_stateful_total(stateful_total);
  const int64_t stateful_free =
      base::SysInfo::AmountOfFreeDiskSpace(stateful_path);
  if (stateful_free >= 0)
    event->set_stateful_free(stateful_free);
  return event;
}

void EnsureTimestampSet(em::ExtensionInstallReportLogEvent* event) {
  if (!event->has_timestamp()) {
    event->set_timestamp(
        (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds());
  }
}

std::unique_ptr<em::ExtensionInstallReportLogEvent> CreateEvent(
    em::ExtensionInstallReportLogEvent::EventType type) {
  auto event = std::make_unique<em::ExtensionInstallReportLogEvent>();
  EnsureTimestampSet(event.get());
  event->set_event_type(type);
  return event;
}

}  // namespace

ExtensionInstallEventLogger::ExtensionInstallEventLogger(
    Delegate* delegate,
    Profile* profile,
    extensions::ExtensionRegistry* registry)
    : delegate_(delegate),
      profile_(profile),
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
    log_collector_->AddLogoutEvent();
  pref_change_registrar_.RemoveAll();
}

void ExtensionInstallEventLogger::AddForAllExtensions(
    std::unique_ptr<em::ExtensionInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  AddForSetOfExtensions(pending_extensions_, std::move(event));
}

void ExtensionInstallEventLogger::Add(
    const extensions::ExtensionId& extension_id,
    bool gather_disk_space_info,
    std::unique_ptr<em::ExtensionInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  if (gather_disk_space_info) {
    AddForSetOfExtensionsWithDiskSpaceInfo({extension_id}, std::move(event));
  } else {
    AddForSetOfExtensions({extension_id}, std::move(event));
  }
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
    AddForSetOfExtensionsWithDiskSpaceInfo(
        added, CreateEvent(em::ExtensionInstallReportLogEvent::POLICY_REQUEST));
  }
  if (!removed.empty()) {
    AddForSetOfExtensions(
        removed, CreateEvent(em::ExtensionInstallReportLogEvent::CANCELED));
  }
  std::set<extensions::ExtensionId> current_pending = GetDifference(
      current_requested, GetDifference(extensions_, previous_pending));
  pending_extensions_ = std::move(current_pending);
  extensions_ = std::move(current_requested);

  if (!pending_extensions_.empty()) {
    UpdateCollector();
    if (initial_) {
      log_collector_->AddLoginEvent();
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

void ExtensionInstallEventLogger::SetStatefulPathForTesting(
    const base::FilePath& path) {
  stateful_path_ = path;
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

void ExtensionInstallEventLogger::AddForSetOfExtensionsWithDiskSpaceInfo(
    const std::set<extensions::ExtensionId>& extensions,
    std::unique_ptr<em::ExtensionInstallReportLogEvent> event) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&AddDiskSpaceInfoToEvent, std::move(event),
                     stateful_path_),
      base::BindOnce(&ExtensionInstallEventLogger::AddForSetOfExtensions,
                     weak_factory_.GetWeakPtr(), extensions));
}

void ExtensionInstallEventLogger::AddForSetOfExtensions(
    const std::set<extensions::ExtensionId>& extensions,
    std::unique_ptr<em::ExtensionInstallReportLogEvent> event) {
  delegate_->Add(extensions, *event);
}

}  // namespace policy
