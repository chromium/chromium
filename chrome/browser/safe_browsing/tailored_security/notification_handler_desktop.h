// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_NOTIFICATION_HANDLER_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_NOTIFICATION_HANDLER_DESKTOP_H_

#include "chrome/browser/notifications/notification_handler.h"

class Profile;

namespace safe_browsing {

// Handler for notifications shown for TailoredSecurity on Desktop platforms.
// Will be created and owned by the NativeNotificationDisplayService.
class TailoredSecurityNotificationHandler : public NotificationHandler {
 public:
  TailoredSecurityNotificationHandler();

  TailoredSecurityNotificationHandler(
      const TailoredSecurityNotificationHandler&) = delete;
  TailoredSecurityNotificationHandler& operator=(
      const TailoredSecurityNotificationHandler&) = delete;

  ~TailoredSecurityNotificationHandler() override;

  // NotificationHandler implementation.
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
};

void DisplayTailoredSecurityUnconsentedPromotionNotification(Profile* profile);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_NOTIFICATION_HANDLER_DESKTOP_H_
