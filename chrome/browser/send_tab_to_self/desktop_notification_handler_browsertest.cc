// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  MOCK_CONST_METHOD1(GetEntryByGUID,
                     const SendTabToSelfEntry*(const std::string&));
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

class DesktopNotificationHandlerBrowserTest : public InProcessBrowserTest {
 public:
  DesktopNotificationHandlerBrowserTest() = default;
  ~DesktopNotificationHandlerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    NotificationDisplayServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestNotificationDisplayService));
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestSendTabToSelfSyncService));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    display_service_mock_ = static_cast<NotificationDisplayServiceMock*>(
        NotificationDisplayServiceFactory::GetForProfile(profile()));
    model_mock_ = static_cast<SendTabToSelfModelMock*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile())
            ->GetSendTabToSelfModel());
  }

  void TearDownOnMainThread() override {
    model_mock_ = nullptr;
    display_service_mock_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return browser()->profile(); }

 protected:
  raw_ptr<SendTabToSelfModelMock> model_mock_;
  raw_ptr<NotificationDisplayServiceMock> display_service_mock_;
};

IN_PROC_BROWSER_TEST_F(DesktopNotificationHandlerBrowserTest,
                       DisplayNewEntries) {
  const GURL& url = GURL(kDesktopNotificationOrigin);
  message_center::RichNotificationData optional_fields;
  optional_fields.never_timeout = true;
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kDesktopNotificationGuid,
      kDesktopNotificationTitle16, kDesktopNotificationDeviceInfoWithPrefix,
      ui::ImageModel(), base::UTF8ToUTF16(url.GetHost()), url,
      message_center::NotifierId(url), optional_fields, /*delegate=*/nullptr);

  SendTabToSelfEntry entry(
      kDesktopNotificationGuid, url, kDesktopNotificationTitle,
      base::Time::Now(), kDesktopNotificationDeviceInfo,
      kDesktopNotificationTargetDeviceSyncCacheGuid, PageContext());
  std::vector<const SendTabToSelfEntry*> entries;
  entries.push_back(&entry);

  DesktopNotificationHandler handler(profile());
  EXPECT_CALL(*display_service_mock_,
              DisplayMockImpl(NotificationHandler::Type::SEND_TAB_TO_SELF,
                              EqualNotification(notification), nullptr));

  handler.DisplayNewEntries(entries);
}

IN_PROC_BROWSER_TEST_F(DesktopNotificationHandlerBrowserTest, DismissEntries) {
  DesktopNotificationHandler handler(profile());
  EXPECT_CALL(*display_service_mock_,
              Close(NotificationHandler::Type::SEND_TAB_TO_SELF,
                    kDesktopNotificationGuid));

  std::vector<std::string> guids;
  guids.push_back(kDesktopNotificationGuid);
  handler.DismissEntries(guids);
}

IN_PROC_BROWSER_TEST_F(DesktopNotificationHandlerBrowserTest, CloseHandler) {
  DesktopNotificationHandler handler(profile());

  EXPECT_CALL(*model_mock_, DismissEntry(kDesktopNotificationId));

  handler.OnClose(profile(), GURL(kDesktopNotificationOrigin),
                  kDesktopNotificationId, /*by_user=*/false, base::DoNothing());

  EXPECT_CALL(*model_mock_, DismissEntry(kDesktopNotificationId));

  handler.OnClose(profile(), GURL(kDesktopNotificationOrigin),
                  kDesktopNotificationId, /*by_user=*/true, base::DoNothing());
}

IN_PROC_BROWSER_TEST_F(DesktopNotificationHandlerBrowserTest, ClickHandler) {
  DesktopNotificationHandler handler(profile());

  EXPECT_CALL(*model_mock_, GetEntryByGUID(kDesktopNotificationId))
      .WillRepeatedly(::testing::Return(nullptr));
  EXPECT_CALL(*display_service_mock_,
              Close(NotificationHandler::Type::SEND_TAB_TO_SELF,
                    kDesktopNotificationId));
  EXPECT_CALL(*model_mock_, MarkEntryOpened(kDesktopNotificationId));

  base::HistogramTester histogram_tester;

  handler.OnClick(profile(), GURL(kDesktopNotificationOrigin),
                  kDesktopNotificationId, /*action_index=*/1,
                  /*reply=*/std::nullopt, base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.NotificationClicked.HasScrollPosition", false, 1);
}

IN_PROC_BROWSER_TEST_F(DesktopNotificationHandlerBrowserTest,
                       ClickHandler_WithScrollPosition) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html");

  DesktopNotificationHandler handler(profile());

  PageContext page_context;
  page_context.scroll_position.text_fragment =
      TextFragmentData("Some text", "", "", "");

  SendTabToSelfEntry entry(
      kDesktopNotificationId, test_url, kDesktopNotificationTitle,
      base::Time::Now(), kDesktopNotificationDeviceInfo,
      kDesktopNotificationTargetDeviceSyncCacheGuid, page_context);

  EXPECT_CALL(*model_mock_, GetEntryByGUID(kDesktopNotificationId))
      .WillRepeatedly(::testing::Return(&entry));
  EXPECT_CALL(*display_service_mock_,
              Close(NotificationHandler::Type::SEND_TAB_TO_SELF,
                    kDesktopNotificationId));
  EXPECT_CALL(*model_mock_, MarkEntryOpened(kDesktopNotificationId));

  base::HistogramTester histogram_tester;

  content::TestNavigationObserver navigation_observer{test_url};
  navigation_observer.StartWatchingNewWebContents();

  handler.OnClick(profile(), test_url, kDesktopNotificationId,
                  /*action_index=*/std::nullopt,
                  /*reply=*/std::nullopt, base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.NotificationClicked.HasScrollPosition", true, 1);

  navigation_observer.Wait();
  EXPECT_EQ(navigation_observer.last_navigation_url(), test_url);

  // Scroll should not happen because the feature is disabled.
  double scroll_y =
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "new Promise(r => { "
                      "  setTimeout(() => r(window.scrollY), 200); "
                      "})")
          .ExtractDouble();
  EXPECT_EQ(scroll_y, 0.0);
}

class DesktopNotificationHandlerScrollPositionBrowserTest
    : public DesktopNotificationHandlerBrowserTest {
 public:
  DesktopNotificationHandlerScrollPositionBrowserTest() {
    feature_list_.InitAndEnableFeature(kSendTabToSelfPropagateScrollPosition);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DesktopNotificationHandlerScrollPositionBrowserTest,
                       ClickHandler_WithScrollPosition) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html");

  DesktopNotificationHandler handler(profile());

  PageContext page_context;
  page_context.scroll_position.text_fragment =
      TextFragmentData("Some text", "", "", "");

  SendTabToSelfEntry entry(
      kDesktopNotificationId, test_url, kDesktopNotificationTitle,
      base::Time::Now(), kDesktopNotificationDeviceInfo,
      kDesktopNotificationTargetDeviceSyncCacheGuid, page_context);

  EXPECT_CALL(*model_mock_, GetEntryByGUID(kDesktopNotificationId))
      .WillRepeatedly(::testing::Return(&entry));
  EXPECT_CALL(*display_service_mock_,
              Close(NotificationHandler::Type::SEND_TAB_TO_SELF,
                    kDesktopNotificationId));
  EXPECT_CALL(*model_mock_, MarkEntryOpened(kDesktopNotificationId));

  base::HistogramTester histogram_tester;

  content::TestNavigationObserver navigation_observer{test_url};
  navigation_observer.StartWatchingNewWebContents();

  handler.OnClick(profile(), test_url, kDesktopNotificationId,
                  /*action_index=*/std::nullopt,
                  /*reply=*/std::nullopt, base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.NotificationClicked.HasScrollPosition", true, 1);

  navigation_observer.Wait();
  EXPECT_EQ(navigation_observer.last_navigation_url(), test_url);

  // Wait for the scroll to be applied and verify it.
  // The text fragment is at (10000, 10000) in the test page.
  // We check that it's within the viewport.
  bool in_viewport =
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "new Promise(r => { "
                      "  const isVisible = () => { "
                      "    const rect = document.getElementById('text')."
                      "getBoundingClientRect(); "
                      "    return rect.top >= 0 && "
                      "rect.top <= window.innerHeight; "
                      "  }; "
                      "  if (isVisible()) r(true); "
                      "  else window.addEventListener('scroll', () => "
                      "r(isVisible()), {once:true}); "
                      "})")
          .ExtractBool();
  EXPECT_TRUE(in_viewport);
}

IN_PROC_BROWSER_TEST_F(DesktopNotificationHandlerScrollPositionBrowserTest,
                       ClickHandler_NonExistentTextFragment) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html");

  DesktopNotificationHandler handler(profile());

  PageContext page_context;
  page_context.scroll_position.text_fragment =
      TextFragmentData("This text does not exist", "", "", "");

  SendTabToSelfEntry entry(
      kDesktopNotificationId, test_url, kDesktopNotificationTitle,
      base::Time::Now(), kDesktopNotificationDeviceInfo,
      kDesktopNotificationTargetDeviceSyncCacheGuid, page_context);

  EXPECT_CALL(*model_mock_, GetEntryByGUID(kDesktopNotificationId))
      .WillRepeatedly(::testing::Return(&entry));
  EXPECT_CALL(*display_service_mock_,
              Close(NotificationHandler::Type::SEND_TAB_TO_SELF,
                    kDesktopNotificationId));
  EXPECT_CALL(*model_mock_, MarkEntryOpened(kDesktopNotificationId));

  base::HistogramTester histogram_tester;

  content::TestNavigationObserver navigation_observer{test_url};
  navigation_observer.StartWatchingNewWebContents();

  handler.OnClick(profile(), test_url, kDesktopNotificationId,
                  /*action_index=*/std::nullopt,
                  /*reply=*/std::nullopt, base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.NotificationClicked.HasScrollPosition", true, 1);

  navigation_observer.Wait();
  EXPECT_EQ(navigation_observer.last_navigation_url(), test_url);

  // Scroll should not happen, verify scrollY is 0 after a short delay to ensure
  // it would have happened if it were going to.
  double scroll_y =
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "new Promise(r => { "
                      "  setTimeout(() => r(window.scrollY), 200); "
                      "})")
          .ExtractDouble();
  EXPECT_EQ(scroll_y, 0.0);
}

IN_PROC_BROWSER_TEST_F(DesktopNotificationHandlerScrollPositionBrowserTest,
                       ClickHandler_EmptyScrollPosition) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html");

  DesktopNotificationHandler handler(profile());

  PageContext page_context;
  // Leave scroll_position empty.

  SendTabToSelfEntry entry(
      kDesktopNotificationId, test_url, kDesktopNotificationTitle,
      base::Time::Now(), kDesktopNotificationDeviceInfo,
      kDesktopNotificationTargetDeviceSyncCacheGuid, page_context);

  EXPECT_CALL(*model_mock_, GetEntryByGUID(kDesktopNotificationId))
      .WillRepeatedly(::testing::Return(&entry));
  EXPECT_CALL(*display_service_mock_,
              Close(NotificationHandler::Type::SEND_TAB_TO_SELF,
                    kDesktopNotificationId));
  EXPECT_CALL(*model_mock_, MarkEntryOpened(kDesktopNotificationId));

  base::HistogramTester histogram_tester;

  content::TestNavigationObserver navigation_observer{test_url};
  navigation_observer.StartWatchingNewWebContents();

  handler.OnClick(profile(), test_url, kDesktopNotificationId,
                  /*action_index=*/std::nullopt,
                  /*reply=*/std::nullopt, base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.NotificationClicked.HasScrollPosition", false, 1);

  navigation_observer.Wait();
  EXPECT_EQ(navigation_observer.last_navigation_url(), test_url);

  double scroll_y =
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "new Promise(r => { "
                      "  setTimeout(() => r(window.scrollY), 200); "
                      "})")
          .ExtractDouble();
  EXPECT_EQ(scroll_y, 0.0);
}

}  // namespace

}  // namespace send_tab_to_self
