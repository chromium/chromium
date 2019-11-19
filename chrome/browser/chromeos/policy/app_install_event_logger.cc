// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_logger.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr int kNonComplianceReasonAppNotInstalled = 5;

std::set<std::string> GetRequestedPackagesFromPolicy(
    const policy::PolicyMap& policy) {
  const base::Value* const arc_enabled = policy.GetValue(key::kArcEnabled);
  if (!arc_enabled || !arc_enabled->is_bool() || !arc_enabled->GetBool()) {
    return {};
  }

  const base::Value* const arc_policy = policy.GetValue(key::kArcPolicy);
  if (!arc_policy || !arc_policy->is_string()) {
    return {};
  }

  return arc::policy_util::GetRequestedPackagesFromArcPolicy(
      arc_policy->GetString());
}

// Return all elements that are members of |first| but not |second|.
std::set<std::string> GetDifference(const std::set<std::string>& first,
                                    const std::set<std::string>& second) {
  std::set<std::string> difference;
  std::set_difference(first.begin(), first.end(), second.begin(), second.end(),
                      std::inserter(difference, difference.end()));
  return difference;
}

std::unique_ptr<em::AppInstallReportLogEvent> AddDiskSpaceInfoToEvent(
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  for (const auto& disk :
       chromeos::disks::DiskMountManager::GetInstance()->disks()) {
    if (!disk.second->IsStatefulPartition()) {
      continue;
    }
    const base::FilePath stateful_path(disk.second->mount_path());
    const int64_t stateful_total =
        base::SysInfo::AmountOfTotalDiskSpace(stateful_path);
    if (stateful_total >= 0) {
      event->set_stateful_total(stateful_total);
    }
    const int64_t stateful_free =
        base::SysInfo::AmountOfFreeDiskSpace(stateful_path);
    if (stateful_free >= 0) {
      event->set_stateful_free(stateful_free);
    }
    break;
  }
  return event;
}

void EnsureTimestampSet(em::AppInstallReportLogEvent* event) {
  if (!event->has_timestamp()) {
    event->set_timestamp(
        (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds());
  }
}

std::unique_ptr<em::AppInstallReportLogEvent> CreateEvent(
    em::AppInstallReportLogEvent::EventType type) {
  std::unique_ptr<em::AppInstallReportLogEvent> event =
      std::make_unique<em::AppInstallReportLogEvent>();
  EnsureTimestampSet(event.get());
  event->set_event_type(type);
  return event;
}

}  // namespace

AppInstallEventLogger::AppInstallEventLogger(Delegate* delegate,
                                             Profile* profile)
    : delegate_(delegate), profile_(profile) {
  if (!arc::IsArcAllowedForProfile(profile_)) {
    AddForSetOfPackages(
        GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending),
        CreateEvent(em::AppInstallReportLogEvent::CANCELED));
    Clear(profile_);
    return;
  }

  policy::PolicyService* const policy_service =
      profile_->GetProfilePolicyConnector()->policy_service();
  EvaluatePolicy(policy_service->GetPolicies(policy::PolicyNamespace(
                     policy::POLICY_DOMAIN_CHROME, std::string())),
                 true /* initial */);

  observing_ = true;
  arc::ArcPolicyBridge* bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  bridge->AddObserver(this);
  policy_service->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
}

AppInstallEventLogger::~AppInstallEventLogger() {
  if (log_collector_) {
    log_collector_->AddLogoutEvent();
  }
  if (observing_) {
    arc::ArcPolicyBridge::GetForBrowserContext(profile_)->RemoveObserver(this);
    profile_->GetProfilePolicyConnector()->policy_service()->RemoveObserver(
        policy::POLICY_DOMAIN_CHROME, this);
  }
}

// static
void AppInstallEventLogger::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsRequested);
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsPending);
}

// static
void AppInstallEventLogger::Clear(Profile* profile) {
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsRequested);
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsPending);
}

void AppInstallEventLogger::AddForAllPackages(
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  AddForSetOfPackages(
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending),
      std::move(event));
}

void AppInstallEventLogger::Add(
    const std::string& package,
    bool gather_disk_space_info,
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  if (gather_disk_space_info) {
    AddForSetOfPackagesWithDiskSpaceInfo({package}, std::move(event));
  } else {
    AddForSetOfPackages({package}, std::move(event));
  }
}

void AppInstallEventLogger::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                            const policy::PolicyMap& previous,
                                            const policy::PolicyMap& current) {
  EvaluatePolicy(current, false /* initial */);
}

void AppInstallEventLogger::OnPolicySent(const std::string& policy) {
  requested_in_arc_ =
      arc::policy_util::GetRequestedPackagesFromArcPolicy(policy);
}

void AppInstallEventLogger::OnComplianceReportReceived(
    const base::Value* compliance_report) {
  const base::Value* const details = compliance_report->FindKeyOfType(
      "nonComplianceDetails", base::Value::Type::LIST);
  if (!details) {
    return;
  }

  const std::set<std::string> previous_pending =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending);

  std::set<std::string> pending_in_arc;
  for (const auto& detail : details->GetList()) {
    const base::Value* const reason =
        detail.FindKeyOfType("nonComplianceReason", base::Value::Type::INTEGER);
    if (!reason || reason->GetInt() != kNonComplianceReasonAppNotInstalled) {
      continue;
    }
    const base::Value* const app_name =
        detail.FindKeyOfType("packageName", base::Value::Type::STRING);
    if (!app_name || app_name->GetString().empty()) {
      continue;
    }
    pending_in_arc.insert(app_name->GetString());
  }
  const std::set<std::string> current_pending = GetDifference(
      previous_pending, GetDifference(requested_in_arc_, pending_in_arc));
  const std::set<std::string> removed =
      GetDifference(previous_pending, current_pending);
  AddForSetOfPackagesWithDiskSpaceInfo(
      removed, CreateEvent(em::AppInstallReportLogEvent::SUCCESS));

  if (removed.empty()) {
    return;
  }

  SetPref(arc::prefs::kArcPushInstallAppsPending, current_pending);

  if (!current_pending.empty()) {
    UpdateCollector(current_pending);
  } else {
    StopCollector();
  }
}

std::set<std::string> AppInstallEventLogger::GetPackagesFromPref(
    const std::string& pref_name) const {
  std::set<std::string> packages;
  for (const auto& package :
       profile_->GetPrefs()->GetList(pref_name)->GetList()) {
    if (!package.is_string()) {
      continue;
    }
    packages.insert(package.GetString());
  }
  return packages;
}

void AppInstallEventLogger::SetPref(const std::string& pref_name,
                                    const std::set<std::string>& packages) {
  base::Value value(base::Value::Type::LIST);
  for (const std::string& package : packages) {
    value.Append(package);
  }
  profile_->GetPrefs()->Set(pref_name, value);
}

void AppInstallEventLogger::UpdateCollector(
    const std::set<std::string>& pending) {
  if (!log_collector_) {
    log_collector_ =
        std::make_unique<AppInstallEventLogCollector>(this, profile_, pending);
  } else {
    log_collector_->OnPendingPackagesChanged(pending);
  }
}

void AppInstallEventLogger::StopCollector() {
  log_collector_.reset();
}

void AppInstallEventLogger::EvaluatePolicy(const policy::PolicyMap& policy,
                                           bool initial) {
  const std::set<std::string> previous_requested =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsRequested);
  const std::set<std::string> previous_pending =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending);

  const std::set<std::string> current_requested =
      GetRequestedPackagesFromPolicy(policy);

  const std::set<std::string> added =
      GetDifference(current_requested, previous_requested);
  const std::set<std::string> removed =
      GetDifference(previous_pending, current_requested);
  AddForSetOfPackagesWithDiskSpaceInfo(
      added, CreateEvent(em::AppInstallReportLogEvent::SERVER_REQUEST));
  AddForSetOfPackages(removed,
                      CreateEvent(em::AppInstallReportLogEvent::CANCELED));

  const std::set<std::string> current_pending = GetDifference(
      current_requested, GetDifference(previous_requested, previous_pending));
  SetPref(arc::prefs::kArcPushInstallAppsRequested, current_requested);
  SetPref(arc::prefs::kArcPushInstallAppsPending, current_pending);

  if (!current_pending.empty()) {
    UpdateCollector(current_pending);
    if (initial) {
      log_collector_->AddLoginEvent();
    }
  } else {
    StopCollector();
  }
}

void AppInstallEventLogger::AddForSetOfPackagesWithDiskSpaceInfo(
    const std::set<std::string>& packages,
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&AddDiskSpaceInfoToEvent, std::move(event)),
      base::BindOnce(&AppInstallEventLogger::AddForSetOfPackages,
                     weak_factory_.GetWeakPtr(), packages));
}

void AppInstallEventLogger::AddForSetOfPackages(
    const std::set<std::string>& packages,
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  delegate_->Add(packages, *event);
}

}  // namespace policy
