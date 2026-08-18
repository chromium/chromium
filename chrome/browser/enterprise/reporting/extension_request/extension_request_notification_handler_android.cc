// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_notification_handler_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/ExtensionRequestNotificationHelper_jni.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_notification.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

namespace enterprise_reporting {

ExtensionRequestNotificationHandler::ExtensionRequestNotificationHandler() =
    default;

ExtensionRequestNotificationHandler::~ExtensionRequestNotificationHandler() =
    default;

void ExtensionRequestNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  if (by_user && profile) {
    std::vector<std::string> extension_ids = ParseExtensionIds(notification_id);
    if (!extension_ids.empty()) {
      ExtensionRequestObserver::RemoveExtensionsFromPendingList(profile,
                                                                extension_ids);
    }
  }
  std::move(completed_closure).Run();
}

void ExtensionRequestNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const std::optional<int>& action_index,
    const std::optional<std::u16string>& reply,
    base::OnceClosure completed_closure) {
  std::vector<std::string> extension_ids = ParseExtensionIds(notification_id);
  if (!extension_ids.empty() && profile) {
    LaunchWebStoreUrls(extension_ids);
    ExtensionRequestObserver::RemoveExtensionsFromPendingList(profile,
                                                              extension_ids);
    if (auto* display_service =
            NotificationDisplayServiceFactory::GetForProfile(profile)) {
      display_service->Close(NotificationHandler::Type::EXTENSION_REQUEST,
                             notification_id);
    }
  }
  std::move(completed_closure).Run();
}

void ExtensionRequestNotificationHandler::LaunchWebStoreUrls(
    const std::vector<std::string>& extension_ids) {
  Java_ExtensionRequestNotificationHelper_launchWebStoreUrls(
      base::android::AttachCurrentThread(), extension_ids);
}

// static
std::vector<std::string> ExtensionRequestNotificationHandler::ParseExtensionIds(
    const std::string& notification_id) {
  return ExtensionRequestNotification::ParseExtensionIds(notification_id);
}

}  // namespace enterprise_reporting
