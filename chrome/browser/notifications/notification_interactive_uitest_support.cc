// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_interactive_uitest_support.h"

#include <vector>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

class MessageCenterChangeObserver::Impl
    : public message_center::MessageCenterObserver {
 public:
  Impl() : notification_received_(false) {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  ~Impl() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  bool Wait() {
    if (notification_received_)
      return true;

    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    return notification_received_;
  }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    OnMessageCenterChanged();
  }

  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override {
    OnMessageCenterChanged();
  }

  void OnNotificationUpdated(const std::string& notification_id) override {
    OnMessageCenterChanged();
  }

  void OnNotificationClicked(
      const std::string& notification_id,
      const std::optional<int>& button_index,
      const std::optional<std::u16string>& reply) override {
    OnMessageCenterChanged();
  }

  void OnMessageCenterChanged() {
    notification_received_ = true;
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  bool notification_received_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

MessageCenterChangeObserver::MessageCenterChangeObserver() : impl_(new Impl) {}

MessageCenterChangeObserver::~MessageCenterChangeObserver() = default;

bool MessageCenterChangeObserver::Wait() {
  return impl_->Wait();
}

void TestMessageCenterObserver::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  last_displayed_id_ = notification_id;
}

const std::string& TestMessageCenterObserver::last_displayed_id() const {
  return last_displayed_id_;
}

NotificationsTest::NotificationsTest() {
// Temporary change while the whole support class is changed to deal
// with system notifications. crbug.com/714679
  feature_list_.InitWithFeatures(
      {}, {features::kNativeNotifications, features::kSystemNotifications});
}

int NotificationsTest::GetNotificationCount() {
  return message_center::MessageCenter::Get()->NotificationCount();
}

int NotificationsTest::GetNotificationPopupCount() {
  return message_center::MessageCenter::Get()->GetPopupNotifications().size();
}

void NotificationsTest::CrashTab(Browser* browser, int index) {
  content::CrashTab(browser->tab_strip_model()->GetWebContentsAt(index));
}

void NotificationsTest::DenyOrigin(const GURL& origin) {
  NotificationPermissionContext::UpdatePermission(browser()->profile(), origin,
                                                  CONTENT_SETTING_BLOCK);
}

void NotificationsTest::AllowOrigin(const GURL& origin) {
  NotificationPermissionContext::UpdatePermission(browser()->profile(), origin,
                                                  CONTENT_SETTING_ALLOW);
}

void NotificationsTest::AllowAllOrigins() {
  // Reset all origins
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::NOTIFICATIONS);
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
}

void NotificationsTest::SetDefaultContentSetting(ContentSetting setting) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS, setting);
}

std::string NotificationsTest::CreateNotification(Browser* browser,
                                                  bool wait_for_new_balloon,
                                                  const char* icon,
                                                  const char* title,
                                                  const char* body,
                                                  const char* replace_id,
                                                  const char* onclick) {
  std::string script = base::StringPrintf(
      "createNotification('%s', '%s', '%s', '%s', (e) => { %s });", icon, title,
      body, replace_id, onclick);

  MessageCenterChangeObserver observer;
  std::string result =
      content::EvalJs(GetActiveWebContents(browser), script).ExtractString();
  if (result != "-1" && wait_for_new_balloon) {
    EXPECT_TRUE(observer.Wait());
  }

  return result;
}

std::string NotificationsTest::CreateSimpleNotification(
    Browser* browser,
    bool wait_for_new_balloon) {
  return CreateNotification(browser, wait_for_new_balloon, "no_such_file.png",
                            "My Title", "My Body", "");
}

std::string NotificationsTest::RequestAndRespondToPermission(
    Browser* browser,
    permissions::PermissionRequestManager::AutoResponseType bubble_response) {
  content::WebContents* web_contents = GetActiveWebContents(browser);
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(bubble_response);
  return content::EvalJs(web_contents, "requestPermission();").ExtractString();
}

bool NotificationsTest::RequestAndAcceptPermission(Browser* browser) {
  std::string result = RequestAndRespondToPermission(
      browser, permissions::PermissionRequestManager::ACCEPT_ALL);
  return "request-callback-granted" == result;
}

bool NotificationsTest::RequestAndDenyPermission(Browser* browser) {
  std::string result = RequestAndRespondToPermission(
      browser, permissions::PermissionRequestManager::DENY_ALL);
  return "request-callback-denied" == result;
}

bool NotificationsTest::RequestAndDismissPermission(Browser* browser) {
  std::string result = RequestAndRespondToPermission(
      browser, permissions::PermissionRequestManager::DISMISS);
  return "request-callback-default" == result;
}

bool NotificationsTest::RequestPermissionAndWait(Browser* browser) {
  content::WebContents* web_contents = GetActiveWebContents(browser);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, GetTestPageURL()));
  permissions::PermissionRequestObserver observer(web_contents);
  EXPECT_EQ("requested",
            content::EvalJs(web_contents, "requestPermissionAndRespond();"));
  observer.Wait();
  return observer.request_shown();
}

std::string NotificationsTest::QueryPermissionStatus(Browser* browser) {
  content::WebContents* web_contents = GetActiveWebContents(browser);
  return content::EvalJs(web_contents, "queryPermissionStatus();")
      .ExtractString();
}

bool NotificationsTest::CancelNotification(const char* notification_id,
                                           Browser* browser) {
  std::string script =
      base::StringPrintf("cancelNotification('%s');", notification_id);

  MessageCenterChangeObserver observer;
  std::string result =
      content::EvalJs(GetActiveWebContents(browser), script).ExtractString();
  if (result != "1") {
    return false;
  }
  return observer.Wait();
}

void NotificationsTest::GetDisabledContentSettings(
    ContentSettingsForOneType* settings) {
  *settings = HostContentSettingsMapFactory::GetForProfile(browser()->profile())
                  ->GetSettingsForOneType(ContentSettingsType::NOTIFICATIONS);

  std::erase_if(*settings, [](const ContentSettingPatternSource& setting) {
    return setting.GetContentSetting() != CONTENT_SETTING_BLOCK ||
           setting.source != content_settings::ProviderType::kPrefProvider;
  });
}

bool NotificationsTest::CheckOriginInSetting(
    const ContentSettingsForOneType& settings,
    const GURL& origin) {
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURLNoWildcard(origin);
  for (const auto& setting : settings) {
    if (setting.primary_pattern == pattern)
      return true;
  }
  return false;
}

GURL NotificationsTest::GetTestPageURLForFile(const std::string& file) const {
  return embedded_test_server()->GetURL(std::string("/notifications/") + file);
}

GURL NotificationsTest::GetTestPageURL() const {
  return GetTestPageURLForFile("notification_tester.html");
}

content::WebContents* NotificationsTest::GetActiveWebContents(
    Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

NotificationsTestWithPermissionsEmbargo ::
    NotificationsTestWithPermissionsEmbargo() {
  feature_list_.InitWithFeatures(
      {permissions::features::kBlockPromptsIfDismissedOften,
       permissions::features::kBlockPromptsIfIgnoredOften},
      {features::kSystemNotifications});
}
