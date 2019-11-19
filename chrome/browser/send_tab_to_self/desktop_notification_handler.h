// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_DESKTOP_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_DESKTOP_NOTIFICATION_HANDLER_H_

#include <string>
#include <vector>

#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"

class Profile;

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Handler for desktop notifications shown by SendTabToSelf.
// Will only be used on desktop platform.
// Will be created and owned by the NativeNotificationDisplayService.
class DesktopNotificationHandler : public NotificationHandler,
                                   public ReceivingUiHandler {
 public:
  explicit DesktopNotificationHandler(Profile* profile);
  ~DesktopNotificationHandler() override;

  // ReceivingUiHandler implementation.
  void DisplayNewEntries(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override;
  void DismissEntries(const std::vector<std::string>& guids) override;

  // NotificationHandler implementation.
  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override;
  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const base::Optional<int>& action_index,
               const base::Optional<base::string16>& reply,
               base::OnceClosure completed_closure) override;

  // When the user share a tab, a confirmation notification will be shown.
  // Displays a notification telling the user that the tab was successfully
  // sent.
  void DisplaySendingConfirmation(const SendTabToSelfEntry& entry,
                                  const std::string& target_device_name);
  // Displays a notification telling the user that the tab could not be sent.
  void DisplayFailureMessage(const GURL& url);

  // Retrieves the Profile for which this Handler will manage notifications.
  const Profile* GetProfile() const;

 protected:
  Profile* const profile_;
  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationHandler);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_DESKTOP_NOTIFICATION_HANDLER_H_
