// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARING_NOTIFICATION_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "url/gurl.h"

class Profile;

// Handles SHARING nofication actions.
class SharingNotificationHandler : public NotificationHandler {
 public:
  SharingNotificationHandler();
  SharingNotificationHandler(const SharingNotificationHandler&) = delete;
  SharingNotificationHandler& operator=(const SharingNotificationHandler&) =
      delete;
  ~SharingNotificationHandler() override;

  // NotificationHandler implementation:
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const std::optional<int>& action_index,
               const std::optional<std::u16string>& reply,
               base::OnceClosure completed_closure) override;
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OpenSettings(Profile* profile, const GURL& origin) override;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_NOTIFICATION_HANDLER_H_
