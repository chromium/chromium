// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
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
  extensions::ExtensionManagementFactory::GetForBrowserContext(profile_)
      ->AddObserver(this);
  OnExtensionManagementSettingsChanged();
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCloudExtensionRequestIds,
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

  // The pending list is updated when user confirm the notification and requests
  // are removed from the list. There is no need to show new notification at
  // this point.
  if (closing_notification_and_deleting_requests_) {
    // Record id removed event.
    base::UmaHistogramEnumeration(kPendingListUpdateMetricsName,
                                  PendlingListUpdateMetricEvent::kRemoved);
    closing_notification_and_deleting_requests_ = false;
    return;
  }
  // Record new id added event.
  base::UmaHistogramEnumeration(kPendingListUpdateMetricsName,
                                PendlingListUpdateMetricEvent::kAdded);
  ShowAllNotifications();
}

void ExtensionRequestObserver::ShowAllNotifications() {
  if (!profile_->GetPrefs()->GetBoolean(prefs::kCloudExtensionRequestEnabled)) {
    CloseAllNotifications();
    return;
  }

  ShowNotification(ExtensionRequestNotification::kApproved);
  ShowNotification(ExtensionRequestNotification::kRejected);
  ShowNotification(ExtensionRequestNotification::kForceInstalled);
}

void ExtensionRequestObserver::ShowNotification(
    ExtensionRequestNotification::NotifyType type) {
  const base::Value::Dict& pending_requests =
      profile_->GetPrefs()->GetDict(prefs::kCloudExtensionRequestIds);

  ExtensionRequestNotification::ExtensionIds filtered_extension_ids;
  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  std::string web_store_update_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();

  for (auto request : pending_requests) {
    const std::string& id = request.first;
    extensions::ExtensionManagement::InstallationMode mode =
        extension_management->GetInstallationMode(id, web_store_update_url);
    if ((type == ExtensionRequestNotification::kApproved &&
         mode == extensions::ExtensionManagement::INSTALLATION_ALLOWED) ||
        (type == ExtensionRequestNotification::kForceInstalled &&
         (mode == extensions::ExtensionManagement::INSTALLATION_FORCED ||
          mode == extensions::ExtensionManagement::INSTALLATION_RECOMMENDED)) ||
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
  notifications_[type]->Show(base::BindOnce(
      &ExtensionRequestObserver::OnNotificationClosed,
      weak_factory_.GetWeakPtr(), std::move(filtered_extension_ids)));
}

void ExtensionRequestObserver::CloseAllNotifications() {
  for (auto& notification : notifications_) {
    if (notification) {
      notification->CloseNotification();
      notification.reset();
    }
  }
}

void ExtensionRequestObserver::OnNotificationClosed(
    std::vector<std::string>&& extension_ids,
    bool by_user) {
  if (!by_user)
    return;

  RemoveExtensionsFromPendingList(extension_ids);
}

void ExtensionRequestObserver::RemoveExtensionsFromPendingList(
    const std::vector<std::string>& extension_ids) {
  ScopedDictPrefUpdate pending_requests_update(
      Profile::FromBrowserContext(profile_)->GetPrefs(),
      prefs::kCloudExtensionRequestIds);
  for (auto& id : extension_ids)
    pending_requests_update->Remove(id);

  closing_notification_and_deleting_requests_ = true;
}

}  // namespace enterprise_reporting
