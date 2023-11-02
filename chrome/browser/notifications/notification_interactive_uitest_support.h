// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_INTERACTIVE_UITEST_SUPPORT_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_INTERACTIVE_UITEST_SUPPORT_H_

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_request_manager.h"
#include "ui/message_center/message_center_observer.h"

class MessageCenterChangeObserver {
 public:
  MessageCenterChangeObserver();
  MessageCenterChangeObserver(const MessageCenterChangeObserver&) = delete;
  MessageCenterChangeObserver& operator=(const MessageCenterChangeObserver&) =
      delete;
  ~MessageCenterChangeObserver();

  bool Wait();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

class TestMessageCenterObserver : public message_center::MessageCenterObserver {
 public:
  TestMessageCenterObserver() = default;
  TestMessageCenterObserver(const TestMessageCenterObserver&) = delete;
  TestMessageCenterObserver& operator=(const TestMessageCenterObserver&) =
      delete;

  // MessageCenterObserver:
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;

  const std::string& last_displayed_id() const;

 private:
  std::string last_displayed_id_;
};

class NotificationsTest : public InProcessBrowserTest {
 public:
  NotificationsTest();

 protected:
  int GetNotificationCount();
  int GetNotificationPopupCount();

  void CloseBrowserWindow(Browser* browser);
  void CrashTab(Browser* browser, int index);

  void DenyOrigin(const GURL& origin);
  void AllowOrigin(const GURL& origin);
  void AllowAllOrigins();
  void SetDefaultContentSetting(ContentSetting setting);
  void DropOriginPreference(const GURL& origin);

  std::string CreateNotification(Browser* browser,
                                 bool wait_for_new_balloon,
                                 const char* icon,
                                 const char* title,
                                 const char* body,
                                 const char* replace_id,
                                 const char* onclick = "");
  std::string CreateSimpleNotification(Browser* browser,
                                       bool wait_for_new_balloon);
  bool RequestAndAcceptPermission(Browser* browser);
  bool RequestAndDenyPermission(Browser* browser);
  bool RequestAndDismissPermission(Browser* browser);
  bool RequestPermissionAndWait(Browser* browser);
  std::string QueryPermissionStatus(Browser* browser);
  bool CancelNotification(const char* notification_id, Browser* browser);
  void GetDisabledContentSettings(ContentSettingsForOneType* settings);
  bool CheckOriginInSetting(const ContentSettingsForOneType& settings,
                            const GURL& origin);

  GURL GetTestPageURLForFile(const std::string& file) const;
  GURL GetTestPageURL() const;
  content::WebContents* GetActiveWebContents(Browser* browser);

 private:
  std::string RequestAndRespondToPermission(
      Browser* browser,
      permissions::PermissionRequestManager::AutoResponseType bubble_response);

  base::test::ScopedFeatureList feature_list_;
};

class NotificationsTestWithPermissionsEmbargo : public NotificationsTest {
 public:
  NotificationsTestWithPermissionsEmbargo();

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_INTERACTIVE_UITEST_SUPPORT_H_
