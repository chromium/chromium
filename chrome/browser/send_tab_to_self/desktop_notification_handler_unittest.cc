// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/strings/grit/ui_strings.h"

namespace send_tab_to_self {

namespace {

const char kDesktopNotificationOrigin[] = "https://www.google.com";
const char kDesktopNotificationId[] = "notification_id";
const char kDesktopNotificationGuid[] = "guid";
const char kDesktopNotificationTitle[] = "title";
const char16_t kDesktopNotificationTitle16[] = u"title";
const char kDesktopNotificationDeviceInfo[] = "device_info";
const char kDesktopNotificationTargetDeviceSyncCacheGuid[] =
    "target_device_sync_cache_guid";
const char16_t kDesktopNotificationDeviceInfoWithPrefix[] =
    u"Shared from device_info";

class SendTabToSelfModelMock : public TestSendTabToSelfModel {
 public:
  SendTabToSelfModelMock() = default;
  ~SendTabToSelfModelMock() override = default;

  MOCK_METHOD1(DeleteEntry, void(const std::string&));
  MOCK_METHOD1(DismissEntry, void(const std::string&));
  MOCK_METHOD1(MarkEntryOpened, void(const std::string&));
};

class TestSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService() : fake_delegate_(syncer::SEND_TAB_TO_SELF) {}

  ~TestSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override { return &model_mock_; }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    return fake_delegate_.GetWeakPtr();
  }

 protected:
  syncer::FakeDataTypeControllerDelegate fake_delegate_;
  SendTabToSelfModelMock model_mock_;
};

// Matcher to compare Notification object
MATCHER_P(EqualNotification, e, "") {
  return arg.type() == e.type() && arg.id() == e.id() &&
         arg.title() == e.title() && arg.message() == e.message() &&
         arg.origin_url() == e.origin_url();
}

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSendTabToSelfSyncService>();
}

class NotificationDisplayServiceMock : public NotificationDisplayService {
 public:
  NotificationDisplayServiceMock() = default;
  ~NotificationDisplayServiceMock() override = default;

  using NotificationDisplayService::DisplayedNotificationsCallback;

  MOCK_METHOD3(DisplayMockImpl,
               void(NotificationHandler::Type,
                    const message_center::Notification&,
                    NotificationCommon::Metadata*));
  void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {
    DisplayMockImpl(notification_type, notification, metadata.get());
  }

  MOCK_METHOD2(Close, void(NotificationHandler::Type, const std::string&));
  MOCK_METHOD1(GetDisplayed, void(DisplayedNotificationsCallback));
  MOCK_METHOD2(GetDisplayedForOrigin,
               void(const GURL& origin, DisplayedNotificationsCallback));
  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));
};

std::unique_ptr<KeyedService> BuildTestNotificationDisplayService(
    content::BrowserContext* context) {
  return std::make_unique<NotificationDisplayServiceMock>();
}

class DesktopNotificationHandlerTest : public BrowserWithTestWindowTest {
 public:
  DesktopNotificationHandlerTest() = default;
  ~DesktopNotificationHandlerTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    display_service_mock_ = static_cast<NotificationDisplayServiceMock*>(
        NotificationDisplayServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(&BuildTestNotificationDisplayService)));

    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(&BuildTestSendTabToSelfSyncService));

    model_mock_ = static_cast<SendTabToSelfModelMock*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile())
            ->GetSendTabToSelfModel());
  }

 protected:
  raw_ptr<SendTabToSelfModelMock, DanglingUntriaged> model_mock_;
  raw_ptr<NotificationDisplayServiceMock, DanglingUntriaged>
      display_service_mock_;
};

TEST_F(DesktopNotificationHandlerTest, DisplayNewEntries) {
  const GURL& url = GURL(kDesktopNotificationOrigin);
  message_center::RichNotificationData optional_fields;
  optional_fields.never_timeout = true;
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kDesktopNotificationGuid,
      kDesktopNotificationTitle16, kDesktopNotificationDeviceInfoWithPrefix,
      ui::ImageModel(), base::UTF8ToUTF16(url.host()), url,
      message_center::NotifierId(url), optional_fields, /*delegate=*/nullptr);

  SendTabToSelfEntry entry(kDesktopNotificationGuid, url,
                           kDesktopNotificationTitle, base::Time::Now(),
                           kDesktopNotificationDeviceInfo,
                           kDesktopNotificationTargetDeviceSyncCacheGuid);
  std::vector<const SendTabToSelfEntry*> entries;
  entries.push_back(&entry);

  DesktopNotificationHandler handler(profile());
  EXPECT_CALL(*display_service_mock_,
              DisplayMockImpl(NotificationHandler::Type::SEND_TAB_TO_SELF,
                              EqualNotification(notification), nullptr))
      .WillOnce(::testing::Return());

  handler.DisplayNewEntries(entries);
}

TEST_F(DesktopNotificationHandlerTest, DismissEntries) {
  DesktopNotificationHandler handler(profile());
  EXPECT_CALL(*display_service_mock_,
              Close(NotificationHandler::Type::SEND_TAB_TO_SELF,
                    kDesktopNotificationGuid))
      .WillOnce(::testing::Return());

  std::vector<std::string> guids;
  guids.push_back(kDesktopNotificationGuid);
  handler.DismissEntries(guids);
}

TEST_F(DesktopNotificationHandlerTest, CloseHandler) {
  DesktopNotificationHandler handler(profile());

  EXPECT_CALL(*model_mock_, DismissEntry(kDesktopNotificationId))
      .WillOnce(::testing::Return());

  handler.OnClose(profile(), GURL(kDesktopNotificationOrigin),
                  kDesktopNotificationId, /*by_user=*/false, base::DoNothing());

  EXPECT_CALL(*model_mock_, DismissEntry(kDesktopNotificationId))
      .WillOnce(::testing::Return());

  handler.OnClose(profile(), GURL(kDesktopNotificationOrigin),
                  kDesktopNotificationId, /*by_user=*/true, base::DoNothing());
}

TEST_F(DesktopNotificationHandlerTest, ClickHandler) {
  DesktopNotificationHandler handler(profile());

  EXPECT_CALL(*display_service_mock_,
              Close(NotificationHandler::Type::SEND_TAB_TO_SELF,
                    kDesktopNotificationId))
      .WillOnce(::testing::Return());
  EXPECT_CALL(*model_mock_, MarkEntryOpened(kDesktopNotificationId))
      .WillOnce(::testing::Return());

  handler.OnClick(profile(), GURL(kDesktopNotificationOrigin),
                  kDesktopNotificationId, /*action_index=*/1,
                  /*reply=*/std::nullopt, base::DoNothing());
}

}  // namespace

}  // namespace send_tab_to_self
