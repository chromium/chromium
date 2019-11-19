// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_SHARING_SHARING_NOTIFICATION_HANDLER_H_

#include <map>
#include <string>

#include "chrome/browser/notifications/notification_handler.h"

// Handles SHARING nofication actions.
class SharingNotificationHandler : public NotificationHandler {
 public:
  SharingNotificationHandler();
  ~SharingNotificationHandler() override;

  // NotificationHandler implementation:
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const base::Optional<int>& action_index,
               const base::Optional<base::string16>& reply,
               base::OnceClosure completed_closure) override;
  void OpenSettings(Profile* profile, const GURL& origin) override;

 protected:
  DISALLOW_COPY_AND_ASSIGN(SharingNotificationHandler);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_NOTIFICATION_HANDLER_H_