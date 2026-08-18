// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_NOTIFICATION_HANDLER_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_NOTIFICATION_HANDLER_ANDROID_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/notifications/notification_handler.h"  // nogncheck crbug.com/40147906

class GURL;
class Profile;

namespace enterprise_reporting {

// Persistent notification handler for enterprise extension requests on Android.
class ExtensionRequestNotificationHandler : public NotificationHandler {
 public:
  ExtensionRequestNotificationHandler();
  ExtensionRequestNotificationHandler(
      const ExtensionRequestNotificationHandler&) = delete;
  ExtensionRequestNotificationHandler& operator=(
      const ExtensionRequestNotificationHandler&) = delete;
  ~ExtensionRequestNotificationHandler() override;

  // NotificationHandler:
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const std::optional<int>& action_index,
               const std::optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;

  static std::vector<std::string> ParseExtensionIds(
      const std::string& notification_id);

 protected:
  // Launches the Chrome Web Store details pages for approved extension IDs.
  // Virtual for testing.
  virtual void LaunchWebStoreUrls(
      const std::vector<std::string>& extension_ids);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_NOTIFICATION_HANDLER_ANDROID_H_
