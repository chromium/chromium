// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/managed_installation_mode.h"
#include "extensions/common/extension_urls.h"

namespace enterprise_reporting {
namespace {

constexpr char kPendingListUpdateMetricsName[] =
    "Enterprise.CloudExtensionRequestUpdated";
enum class PendlingListUpdateMetricEvent {
  kAdded = 0,
  kRemoved = 1,
  kMaxValue = kRemoved
};

}  // namespace

ExtensionRequestObserver::ExtensionRequestObserver(Profile* profile)
    : profile_(profile) {
  previous_pending_requests_ =
      profile_->GetPrefs()
          ->GetDict(enterprise_reporting::kCloudExtensionRequestIds)
          .Clone();
  extensions::ExtensionManagementFactory::GetForBrowserContext(profile_)
      ->AddObserver(this);
  OnExtensionManagementSettingsChanged();
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      enterprise_reporting::kCloudExtensionRequestIds,
      base::BindRepeating(&ExtensionRequestObserver::OnPendingListChanged,
                          weak_factory_.GetWeakPtr()));
}

ExtensionRequestObserver::~ExtensionRequestObserver() {
  // If ExtensionManagement is still available during shutdown
  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  if (extension_management) {
    extension_management->RemoveObserver(this);
  }
  CloseAllNotifications();
}

bool ExtensionRequestObserver::IsReportEnabled() {
  return !report_trigger_.is_null();
}

void ExtensionRequestObserver::EnableReport(ReportTrigger trigger) {
  report_trigger_ = trigger;
}

void ExtensionRequestObserver::DisableReport() {
  report_trigger_.Reset();
}

void ExtensionRequestObserver::OnExtensionManagementSettingsChanged() {
  ShowAllNotifications();
}

void ExtensionRequestObserver::OnPendingListChanged() {
  if (report_trigger_)
    report_trigger_.Run(profile_.get());

  const base::DictValue& current_requests = profile_->GetPrefs()->GetDict(
      enterprise_reporting::kCloudExtensionRequestIds);

  bool has_added =
      std::ranges::any_of(current_requests, [this](const auto& entry) {
        return !previous_pending_requests_.contains(entry.first);
      });

  bool has_removed = std::ranges::any_of(
      previous_pending_requests_, [&current_requests](const auto& entry) {
        return !current_requests.contains(entry.first);
      });

  if (has_removed) {
    base::UmaHistogramEnumeration(kPendingListUpdateMetricsName,
                                  PendlingListUpdateMetricEvent::kRemoved);
  }
  if (has_added) {
    base::UmaHistogramEnumeration(kPendingListUpdateMetricsName,
                                  PendlingListUpdateMetricEvent::kAdded);
    ShowAllNotifications();
  }

  previous_pending_requests_ = current_requests.Clone();
}

void ExtensionRequestObserver::ShowAllNotifications() {
  if (!profile_->GetPrefs()->GetBoolean(
          enterprise_reporting::kCloudExtensionRequestEnabled)) {
    CloseAllNotifications();
    return;
  }

  ShowNotification(ExtensionRequestNotification::kApproved);
  ShowNotification(ExtensionRequestNotification::kRejected);
  ShowNotification(ExtensionRequestNotification::kForceInstalled);
}

void ExtensionRequestObserver::ShowNotification(
    ExtensionRequestNotification::NotifyType type) {
  const base::DictValue& pending_requests = profile_->GetPrefs()->GetDict(
      enterprise_reporting::kCloudExtensionRequestIds);

  ExtensionRequestNotification::ExtensionIds filtered_extension_ids;
  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  std::string web_store_update_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();

  for (auto request : pending_requests) {
    const std::string& id = request.first;
    extensions::ManagedInstallationMode mode =
        extension_management->GetInstallationMode(id, web_store_update_url);
    if ((type == ExtensionRequestNotification::kApproved &&
         mode == extensions::ManagedInstallationMode::kAllowed) ||
        (type == ExtensionRequestNotification::kForceInstalled &&
         (mode == extensions::ManagedInstallationMode::kForced ||
          mode == extensions::ManagedInstallationMode::kRecommended)) ||
        (type == ExtensionRequestNotification::kRejected &&
         extension_management->IsInstallationExplicitlyBlocked(id))) {
      filtered_extension_ids.push_back(id);
    }
  }

  if (filtered_extension_ids.size() == 0) {
    // Any existing notification will be closed.
    if (notifications_[type]) {
      notifications_[type]->CloseNotification();
      notifications_[type].reset();
    }
    return;
  }

  // Open a new notification, notification with same type will be replaced if
  // exists.
  notifications_[type] = std::make_unique<ExtensionRequestNotification>(
      profile_, type, filtered_extension_ids);
#if BUILDFLAG(IS_ANDROID)
  notifications_[type]->Show(base::DoNothing());
#else
  notifications_[type]->Show(base::BindOnce(
      &ExtensionRequestObserver::OnNotificationClosed,
      weak_factory_.GetWeakPtr(), std::move(filtered_extension_ids)));
#endif
}

void ExtensionRequestObserver::CloseAllNotifications() {
  for (auto& notification : notifications_) {
    if (notification) {
      notification->CloseNotification();
      notification.reset();
    }
  }
}

#if !BUILDFLAG(IS_ANDROID)
void ExtensionRequestObserver::OnNotificationClosed(
    std::vector<std::string>&& extension_ids,
    bool by_user) {
  if (!by_user)
    return;

  RemoveExtensionsFromPendingList(profile_, extension_ids);
}
#endif

// static
void ExtensionRequestObserver::RemoveExtensionsFromPendingList(
    Profile* profile,
    const std::vector<std::string>& extension_ids) {
  ScopedDictPrefUpdate pending_requests_update(
      profile->GetPrefs(), enterprise_reporting::kCloudExtensionRequestIds);
  for (const auto& id : extension_ids) {
    pending_requests_update->Remove(id);
  }
}

}  // namespace enterprise_reporting
