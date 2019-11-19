// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_notification_android.h"

#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/browser/permissions/adaptive_notification_permission_ui_selector.h"
#include "chrome/browser/permissions/mock_permission_request.h"
#include "chrome/browser/permissions/permission_features.h"
#include "chrome/browser/permissions/permission_prompt_android.h"
#include "chrome/browser/ui/permission_bubble/permission_prompt.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kExampleHost[] = "example.com";
constexpr char kExampleUrl[] = "http://example.com/";
constexpr char kPermissionRequestTitle[] = "test";
}  // namespace

class MockPermissionPromptDelegate : public PermissionPrompt::Delegate {
 public:
  MockPermissionPromptDelegate()
      : permission_request_(kPermissionRequestTitle,
                            PermissionRequestType::PERMISSION_NOTIFICATIONS,
                            GURL(kExampleUrl)) {
    requests_.push_back(&permission_request_);
  }

  MOCK_METHOD0(Accept, void());
  MOCK_METHOD0(Closing, void());
  MOCK_METHOD0(Deny, void());
  MOCK_METHOD0(GetDisplayNameOrOrigin, PermissionPrompt::DisplayNameOrOrigin());

  const std::vector<PermissionRequest*>& Requests() override {
    return requests_;
  }

 private:
  MockPermissionRequest permission_request_;
  std::vector<PermissionRequest*> requests_;
};

class PermissionRequestNotificationAndroidTest : public testing::Test {
 protected:
  PermissionRequestNotificationAndroidTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~PermissionRequestNotificationAndroidTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Testing Profile");
    test_web_contents_ = test_web_contents_factory_.CreateWebContents(profile_);
    notification_display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_);
  }

  TestingProfile* profile() { return profile_; }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* profile_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory test_web_contents_factory_;
  content::WebContents* test_web_contents_;
  std::unique_ptr<NotificationDisplayServiceTester>
      notification_display_service_tester_;
  MockPermissionPromptDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestNotificationAndroidTest);
};

TEST_F(PermissionRequestNotificationAndroidTest, Create_ReturnsObject) {
  EXPECT_TRUE(PermissionRequestNotificationAndroid::Create(test_web_contents_,
                                                           &delegate_)
                  .get());
}

TEST_F(PermissionRequestNotificationAndroidTest, Create_DisplaysNotification) {
  std::unique_ptr<PermissionRequestNotificationAndroid>
      permissions_notification_prompt_android =
          PermissionRequestNotificationAndroid::Create(test_web_contents_,
                                                       &delegate_);

  std::vector<message_center::Notification> notifications =
      notification_display_service_tester_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::PERMISSION_REQUEST);

  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
            notifications[0].type());
  EXPECT_EQ(PermissionRequestNotificationAndroid::NotificationIdForOrigin(
                kExampleUrl),
            notifications[0].id());
  EXPECT_EQ(base::UTF8ToUTF16(kPermissionRequestTitle),
            notifications[0].title());
  EXPECT_EQ(base::UTF8ToUTF16(kPermissionRequestTitle),
            notifications[0].message());
  EXPECT_EQ(GURL(kExampleUrl), notifications[0].origin_url());
  EXPECT_EQ(base::UTF8ToUTF16(kExampleHost), notifications[0].display_source());
  EXPECT_EQ(message_center::NotifierId(GURL(kExampleUrl)),
            notifications[0].notifier_id());
  EXPECT_EQ(message_center::NotificationPriority::DEFAULT_PRIORITY,
            notifications[0].priority());
  EXPECT_EQ(0u, notifications[0].vibration_pattern().size());
  EXPECT_FALSE(notifications[0].renotify());
  EXPECT_EQ(1u, notifications[0].buttons().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON),
            notifications[0].buttons()[0].title);
  EXPECT_FALSE(notifications[0].should_show_settings_button());
  EXPECT_FALSE(notifications[0].should_show_snooze_button());
}

TEST_F(PermissionRequestNotificationAndroidTest,
       ClickAccept_CallsDelegateAccept) {
  std::unique_ptr<PermissionRequestNotificationAndroid>
      permissions_notification_prompt_android =
          PermissionRequestNotificationAndroid::Create(test_web_contents_,
                                                       &delegate_);

  EXPECT_CALL(delegate_, Accept).Times(1);

  notification_display_service_tester_->SimulateClick(
      NotificationHandler::Type::PERMISSION_REQUEST,
      PermissionRequestNotificationAndroid::NotificationIdForOrigin(
          kExampleUrl),
      0, base::nullopt);
}

TEST_F(PermissionRequestNotificationAndroidTest,
       ClickOther_CallsDelegateClosing) {
  std::unique_ptr<PermissionRequestNotificationAndroid>
      permissions_notification_prompt_android =
          PermissionRequestNotificationAndroid::Create(test_web_contents_,
                                                       &delegate_);

  EXPECT_CALL(delegate_, Closing).Times(1);

  notification_display_service_tester_->SimulateClick(
      NotificationHandler::Type::PERMISSION_REQUEST,
      PermissionRequestNotificationAndroid::NotificationIdForOrigin(
          kExampleUrl),
      1, base::nullopt);
}

TEST_F(PermissionRequestNotificationAndroidTest, Closing_CallsDelegateClosing) {
  std::unique_ptr<PermissionRequestNotificationAndroid>
      permissions_notification_prompt_android =
          PermissionRequestNotificationAndroid::Create(test_web_contents_,
                                                       &delegate_);

  EXPECT_CALL(delegate_, Closing).Times(1);

  notification_display_service_tester_->RemoveNotification(
      NotificationHandler::Type::PERMISSION_REQUEST,
      PermissionRequestNotificationAndroid::NotificationIdForOrigin(
          kExampleUrl),
      true);
}

TEST_F(PermissionRequestNotificationAndroidTest, ShouldShowAsNotification) {
  EXPECT_FALSE(PermissionRequestNotificationAndroid::ShouldShowAsNotification(
      profile(), ContentSettingsType::NOTIFICATIONS));
  EXPECT_FALSE(PermissionRequestNotificationAndroid::ShouldShowAsNotification(
      profile(), ContentSettingsType::GEOLOCATION));

  base::FieldTrialParams params;
  params[kQuietNotificationPromptsUIFlavorParameterName] =
      kQuietNotificationPromptsHeadsUpNotification;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kQuietNotificationPrompts, params);
  AdaptiveNotificationPermissionUiSelector::GetForProfile(profile())
      ->set_should_show_quiet_ui_for_testing(true);

  EXPECT_TRUE(PermissionRequestNotificationAndroid::ShouldShowAsNotification(
      profile(), ContentSettingsType::NOTIFICATIONS));
  EXPECT_FALSE(PermissionRequestNotificationAndroid::ShouldShowAsNotification(
      profile(), ContentSettingsType::GEOLOCATION));
}
