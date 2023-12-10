// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>

#include "ash/components/arc/arc_prefs.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/policy/reporting/install_event_logger_base.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

constexpr int kNonComplianceReasonAppNotInstalled = 5;

std::set<std::string> GetRequestedPackagesFromPolicy(const PolicyMap& policy) {
  const base::Value* const arc_enabled =
      policy.GetValue(key::kArcEnabled, base::Value::Type::BOOLEAN);
  if (!arc_enabled || !arc_enabled->GetBool())
    return {};

  const base::Value* const arc_policy =
      policy.GetValue(key::kArcPolicy, base::Value::Type::STRING);
  if (!arc_policy)
    return {};

  return arc::policy_util::GetRequestedPackagesFromArcPolicy(
      arc_policy->GetString());
}

}  // namespace

ArcAppInstallEventLogger::ArcAppInstallEventLogger(Delegate* delegate,
                                                   Profile* profile)
    : InstallEventLoggerBase(profile), delegate_(delegate) {
  if (!arc::IsArcAllowedForProfile(profile_)) {
    AddForSetOfApps(GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending),
                    CreateEvent(em::AppInstallReportLogEvent::CANCELED));
    Clear(profile_);
    return;
  }

  PolicyService* const policy_service =
      profile_->GetProfilePolicyConnector()->policy_service();
  EvaluatePolicy(policy_service->GetPolicies(
                     PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())),
                 true /* initial */);

  observing_ = true;
  arc::ArcPolicyBridge* bridge =
      arc::ArcPolicyBridge::GetForBrowserContext(profile_);
  bridge->AddObserver(this);
  policy_service->AddObserver(POLICY_DOMAIN_CHROME, this);
}

ArcAppInstallEventLogger::~ArcAppInstallEventLogger() {
  if (log_collector_) {
    log_collector_->OnLogout();
  }
  if (observing_) {
    arc::ArcPolicyBridge::GetForBrowserContext(profile_)->RemoveObserver(this);
    profile_->GetProfilePolicyConnector()->policy_service()->RemoveObserver(
        POLICY_DOMAIN_CHROME, this);
  }
}

// static
void ArcAppInstallEventLogger::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsRequested);
  registry->RegisterListPref(arc::prefs::kArcPushInstallAppsPending);
}

// static
void ArcAppInstallEventLogger::Clear(Profile* profile) {
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsRequested);
  profile->GetPrefs()->ClearPref(arc::prefs::kArcPushInstallAppsPending);
}

void ArcAppInstallEventLogger::AddForAllPackages(
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  EnsureTimestampSet(event.get());
  AddForSetOfApps(GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending),
                  std::move(event));
}

void ArcAppInstallEventLogger::Add(
    const std::string& package,
    bool gather_disk_space_info,
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  AddEvent(package, gather_disk_space_info, event);
}

void ArcAppInstallEventLogger::UpdatePolicySuccessRate(
    const std::string& package,
    bool success) {
  policy_data_helper_.UpdatePolicySuccessRate(package, success);
}

void ArcAppInstallEventLogger::OnPolicyUpdated(const PolicyNamespace& ns,
                                               const PolicyMap& previous,
                                               const PolicyMap& current) {
  EvaluatePolicy(current, false /* initial */);
}

void ArcAppInstallEventLogger::OnPolicySent(const std::string& policy) {
  requested_in_arc_ =
      arc::policy_util::GetRequestedPackagesFromArcPolicy(policy);
}

void ArcAppInstallEventLogger::OnComplianceReportReceived(
    const base::Value* compliance_report) {
  const base::Value::List* const details =
      compliance_report->GetDict().FindList("nonComplianceDetails");
  if (!details) {
    return;
  }

  const std::set<std::string> all_force_install_apps_in_policy =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsRequested);
  const std::set<std::string> previous_pending =
      GetPackagesFromPref(arc::prefs::kArcPushInstallAppsPending);

  std::set<std::string> noncompliant_apps_in_report;
  for (const auto& detail : *details) {
    const base::Value::Dict& details_dict = detail.GetDict();
    const std::optional<int> reason =
        details_dict.FindInt("nonComplianceReason");
    if (!reason || *reason != kNonComplianceReasonAppNotInstalled) {
      continue;
    }
    const std::string* const app_name = details_dict.FindString("packageName");
    if (!app_name || app_name->empty()) {
      continue;
    }
    noncompliant_apps_in_report.insert(*app_name);
  }
  const std::set<std::string> all_installed_apps = GetDifference(
      all_force_install_apps_in_policy, noncompliant_apps_in_report);

  std::set<std::string> newly_installed_apps;
  std::set_intersection(
      previous_pending.begin(), previous_pending.end(),
      all_installed_apps.begin(), all_installed_apps.end(),
      std::inserter(newly_installed_apps, newly_installed_apps.end()));

  AddForSetOfAppsWithDiskSpaceInfo(
      newly_installed_apps, CreateEvent(em::AppInstallReportLogEvent::SUCCESS));

  if (newly_installed_apps.empty()) {
    return;
  }

  SetPref(arc::prefs::kArcPushInstallAppsPending, noncompliant_apps_in_report);

  if (!noncompliant_apps_in_report.empty()) {
    UpdateCollector(noncompliant_apps_in_report);
  } else {
    StopCollector();
  }
}

std::set<std::string> ArcAppInstallEventLogger::GetPackagesFromPref(
    const std::string& pref_name) const {
  std::set<std::string> packages;
  for (const auto& package : profile_->GetPrefs()->GetList(pref_name)) {
    if (!package.is_string()) {
      continue;
    }
    packages.insert(package.GetString());
  }
  return packages;
}

void ArcAppInstallEventLogger::SetPref(const std::string& pref_name,
                                       const std::set<std::string>& packages) {
  base::Value::List value;
  for (const std::string& package : packages) {
    value.Append(package);
  }
  profile_->GetPrefs()->SetList(pref_name, std::move(value));
}

void ArcAppInstallEventLogger::UpdateCollector(
    const std::set<std::string>& pending) {
  if (!log_collector_) {
    log_collector_ = std::make_unique<ArcAppInstallEventLogCollector>(
        this, profile_, pending);
  } else {
    log_collector_->OnPendingPackagesChanged(pending);
  }
}

void ArcAppInstallEventLogger::StopCollector() {
  log_collector_.reset();
}

void ArcAppInstallEventLogger::EvaluatePolicy(const PolicyMap& policy,
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
  AddForSetOfAppsWithDiskSpaceInfo(
      added, CreateEvent(em::AppInstallReportLogEvent::SERVER_REQUEST));
  AddForSetOfApps(removed, CreateEvent(em::AppInstallReportLogEvent::CANCELED));
  // Consider canceled packages as successful since they are not needed
  policy_data_helper_.UpdatePolicySuccessRateForPackages(removed,
                                                         /* success */ true);

  const std::set<std::string> previously_installed =
      GetDifference(previous_requested, previous_pending);
  const std::set<std::string> current_pending =
      GetDifference(current_requested, previously_installed);
  SetPref(arc::prefs::kArcPushInstallAppsRequested, current_requested);
  SetPref(arc::prefs::kArcPushInstallAppsPending, current_pending);

  policy_data_helper_.AddPolicyData(current_pending,
                                    previously_installed.size());

  if (!current_pending.empty()) {
    UpdateCollector(current_pending);
    if (initial) {
      log_collector_->OnLogin();
    }
  } else {
    StopCollector();
  }
}

void ArcAppInstallEventLogger::AddForSetOfApps(
    const std::set<std::string>& packages,
    std::unique_ptr<em::AppInstallReportLogEvent> event) {
  delegate_->GetAndroidId(
      base::BindOnce(&ArcAppInstallEventLogger::OnGetAndroidId,
                     weak_factory_.GetWeakPtr(), packages, std::move(event)));
}

void ArcAppInstallEventLogger::OnGetAndroidId(
    const std::set<std::string>& packages,
    std::unique_ptr<em::AppInstallReportLogEvent> event,
    bool ok,
    int64_t android_id) {
  if (ok) {
    event->set_android_id(android_id);
  }
  delegate_->Add(packages, *event);
}

}  // namespace policy
