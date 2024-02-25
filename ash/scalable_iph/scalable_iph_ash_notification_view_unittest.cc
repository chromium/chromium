// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scalable_iph/scalable_iph_ash_notification_view.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/label.h"

namespace ash {

class ScalableIphAshNotificationViewTest : public AshTestBase {
 public:
  ScalableIphAshNotificationViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ScalableIphAshNotificationViewTest() override = default;

 protected:
  message_center::Notification CreateNotification() {
    message_center::RichNotificationData rich_notification_data;
    return message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, "notification_id",
        u"test_title", u"test message", ui::ImageModel(),
        /*display_source=*/std::u16string(), GURL(),
        message_center::NotifierId(
            message_center::NotifierType::SYSTEM_COMPONENT, "notifier_id",
            NotificationCatalogName::kScalableIphNotification),
        rich_notification_data, new message_center::NotificationDelegate());
  }
};

TEST_F(ScalableIphAshNotificationViewTest, NotificationHasSummaryText) {
  auto notification = CreateNotification();
  auto message_view =
      ScalableIphAshNotificationView::CreateView(notification,
                                                 /*shown_in_popup=*/true);
  auto* notification_view =
      static_cast<ScalableIphAshNotificationView*>(message_view.get());
  auto* header_row = notification_view->GetHeaderRowForTesting();
  // Notification should have the expected summary text.
  EXPECT_EQ(scalable_iph::kNotificationSummaryText,
            header_row->summary_text_for_testing()->GetText());
}

}  // namespace ash
