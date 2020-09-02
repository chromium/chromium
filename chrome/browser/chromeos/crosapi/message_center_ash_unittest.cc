// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/message_center_ash.h"

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using gfx::test::AreBitmapsEqual;
using gfx::test::AreImagesEqual;

namespace crosapi {
namespace {

class MojoDelegate : public mojom::NotificationDelegate {
 public:
  MojoDelegate() = default;
  MojoDelegate(const MojoDelegate&) = delete;
  MojoDelegate& operator=(const MojoDelegate&) = delete;
  ~MojoDelegate() override = default;

  // crosapi::mojom::NotificationDelegate:
  void OnNotificationClosed(bool by_user) override { ++closed_count_; }
  void OnNotificationClicked() override { ++clicked_count_; }
  void OnNotificationButtonClicked(uint32_t button_index) override {
    ++button_clicked_count_;
    last_button_index_ = button_index;
  }
  void OnNotificationSettingsButtonClicked() override {
    ++settings_button_clicked_count_;
  }
  void OnNotificationDisabled() override { ++disabled_count_; }

  // Public because this is test code.
  int closed_count_ = 0;
  int clicked_count_ = 0;
  int button_clicked_count_ = 0;
  uint32_t last_button_index_ = 0;
  int settings_button_clicked_count_ = 0;
  int disabled_count_ = 0;
  mojo::Receiver<mojom::NotificationDelegate> receiver_{this};
};

class MessageCenterAshTest : public testing::Test {
 public:
  MessageCenterAshTest() = default;
  MessageCenterAshTest(const MessageCenterAshTest&) = delete;
  MessageCenterAshTest& operator=(const MessageCenterAshTest&) = delete;
  ~MessageCenterAshTest() override = default;

  // testing::Test:
  void SetUp() override {
    message_center::MessageCenter::Initialize();
    message_center_ash_ = std::make_unique<MessageCenterAsh>(
        message_center_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    message_center_ash_.reset();
    message_center::MessageCenter::Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::MessageCenter> message_center_remote_;
  std::unique_ptr<MessageCenterAsh> message_center_ash_;
};

TEST_F(MessageCenterAshTest, SerializationSimple) {
  // Create a notification.
  auto mojo_notification = mojom::Notification::New();
  mojo_notification->type = mojom::NotificationType::kSimple;
  mojo_notification->id = "test_id";
  mojo_notification->title = ASCIIToUTF16("title");
  mojo_notification->message = ASCIIToUTF16("message");
  mojo_notification->display_source = ASCIIToUTF16("source");
  mojo_notification->origin_url = GURL("http://example.com/");
  mojo_notification->priority = 2;
  mojo_notification->require_interaction = true;
  base::Time now = base::Time::Now();
  mojo_notification->timestamp = now;
  mojo_notification->renotify = true;
  mojo_notification->accessible_name = ASCIIToUTF16("accessible_name");
  mojo_notification->fullscreen_visibility =
      mojom::FullscreenVisibility::kOverUser;

  SkBitmap test_badge = gfx::test::CreateBitmap(1, 2);
  mojo_notification->badge = gfx::ImageSkia::CreateFrom1xBitmap(test_badge);
  SkBitmap test_icon = gfx::test::CreateBitmap(3, 4);
  mojo_notification->icon = gfx::ImageSkia::CreateFrom1xBitmap(test_icon);

  auto button1 = mojom::ButtonInfo::New();
  button1->title = ASCIIToUTF16("button1");
  mojo_notification->buttons.push_back(std::move(button1));
  auto button2 = mojom::ButtonInfo::New();
  button2->title = ASCIIToUTF16("button2");
  mojo_notification->buttons.push_back(std::move(button2));

  // Display the notification.
  MojoDelegate mojo_delegate;
  message_center_remote_->DisplayNotification(
      std::move(mojo_notification),
      mojo_delegate.receiver_.BindNewPipeAndPassRemote());
  message_center_remote_.FlushForTesting();

  // Notification exists and has correct fields.
  auto* message_center = message_center::MessageCenter::Get();
  message_center::Notification* ui_notification =
      message_center->FindVisibleNotificationById("test_id");
  ASSERT_TRUE(ui_notification);
  EXPECT_EQ("test_id", ui_notification->id());
  EXPECT_EQ(ASCIIToUTF16("title"), ui_notification->title());
  EXPECT_EQ(ASCIIToUTF16("message"), ui_notification->message());
  EXPECT_EQ(ASCIIToUTF16("source"), ui_notification->display_source());
  EXPECT_EQ("http://example.com/", ui_notification->origin_url().spec());
  EXPECT_EQ(2, ui_notification->priority());
  EXPECT_TRUE(ui_notification->never_timeout());
  EXPECT_EQ(now, ui_notification->timestamp());
  EXPECT_TRUE(ui_notification->renotify());
  EXPECT_EQ(ASCIIToUTF16("accessible_name"),
            ui_notification->accessible_name());
  EXPECT_EQ(message_center::FullscreenVisibility::OVER_USER,
            ui_notification->fullscreen_visibility());

  EXPECT_TRUE(
      AreBitmapsEqual(test_badge, ui_notification->small_image().AsBitmap()));
  EXPECT_TRUE(AreBitmapsEqual(test_icon, ui_notification->icon().AsBitmap()));

  ASSERT_EQ(2u, ui_notification->buttons().size());
  EXPECT_EQ(ASCIIToUTF16("button1"), ui_notification->buttons()[0].title);
  EXPECT_EQ(ASCIIToUTF16("button2"), ui_notification->buttons()[1].title);
}

TEST_F(MessageCenterAshTest, SerializationImage) {
  // Create a notification with an image.
  auto mojo_notification = mojom::Notification::New();
  mojo_notification->type = mojom::NotificationType::kImage;
  mojo_notification->id = "test_id";

  SkBitmap test_image = gfx::test::CreateBitmap(5, 6);
  mojo_notification->image = gfx::ImageSkia::CreateFrom1xBitmap(test_image);

  // Display the notification.
  MojoDelegate mojo_delegate;
  message_center_remote_->DisplayNotification(
      std::move(mojo_notification),
      mojo_delegate.receiver_.BindNewPipeAndPassRemote());
  message_center_remote_.FlushForTesting();

  // Notification exists and has correct fields.
  auto* message_center = message_center::MessageCenter::Get();
  message_center::Notification* ui_notification =
      message_center->FindVisibleNotificationById("test_id");
  ASSERT_TRUE(ui_notification);
  EXPECT_TRUE(AreBitmapsEqual(test_image, ui_notification->image().AsBitmap()));
}

TEST_F(MessageCenterAshTest, HighDpiImage) {
  // Create a notification with an image.
  auto mojo_notification = mojom::Notification::New();
  mojo_notification->type = mojom::NotificationType::kImage;
  mojo_notification->id = "test_id";

  // Create a high DPI image.
  SkBitmap bitmap = gfx::test::CreateBitmap(2, 4);
  gfx::ImageSkia high_dpi_image_skia(gfx::ImageSkiaRep(bitmap, 2.0f));
  mojo_notification->image = high_dpi_image_skia;

  // Display the notification.
  MojoDelegate mojo_delegate;
  message_center_remote_->DisplayNotification(
      std::move(mojo_notification),
      mojo_delegate.receiver_.BindNewPipeAndPassRemote());
  message_center_remote_.FlushForTesting();

  // Notification exists and has the high DPI image.
  auto* message_center = message_center::MessageCenter::Get();
  message_center::Notification* ui_notification =
      message_center->FindVisibleNotificationById("test_id");
  ASSERT_TRUE(ui_notification);
  EXPECT_TRUE(AreImagesEqual(gfx::Image(high_dpi_image_skia),
                             ui_notification->image()));
}

TEST_F(MessageCenterAshTest, SerializationList) {
  // Create a notification with some list items.
  auto mojo_notification = mojom::Notification::New();
  mojo_notification->type = mojom::NotificationType::kList;
  mojo_notification->id = "test_id";

  auto item1 = mojom::NotificationItem::New();
  item1->title = ASCIIToUTF16("title1");
  item1->message = ASCIIToUTF16("message1");
  mojo_notification->items.push_back(std::move(item1));
  auto item2 = mojom::NotificationItem::New();
  item2->title = ASCIIToUTF16("title2");
  item2->message = ASCIIToUTF16("message2");
  mojo_notification->items.push_back(std::move(item2));

  // Display the notification.
  MojoDelegate mojo_delegate;
  message_center_remote_->DisplayNotification(
      std::move(mojo_notification),
      mojo_delegate.receiver_.BindNewPipeAndPassRemote());
  message_center_remote_.FlushForTesting();

  // Notification exists and has correct fields.
  auto* message_center = message_center::MessageCenter::Get();
  message_center::Notification* ui_notification =
      message_center->FindVisibleNotificationById("test_id");
  ASSERT_TRUE(ui_notification);
  ASSERT_EQ(2u, ui_notification->items().size());
  EXPECT_EQ(ASCIIToUTF16("title1"), ui_notification->items()[0].title);
  EXPECT_EQ(ASCIIToUTF16("message1"), ui_notification->items()[0].message);
  EXPECT_EQ(ASCIIToUTF16("title2"), ui_notification->items()[1].title);
  EXPECT_EQ(ASCIIToUTF16("message2"), ui_notification->items()[1].message);
}

TEST_F(MessageCenterAshTest, SerializationProgress) {
  // Create a notification with partial progress.
  auto mojo_notification = mojom::Notification::New();
  mojo_notification->type = mojom::NotificationType::kProgress;
  mojo_notification->id = "test_id";
  mojo_notification->progress = 55;
  mojo_notification->progress_status = ASCIIToUTF16("status");

  // Display the notification.
  MojoDelegate mojo_delegate1;
  message_center_remote_->DisplayNotification(
      std::move(mojo_notification),
      mojo_delegate1.receiver_.BindNewPipeAndPassRemote());
  message_center_remote_.FlushForTesting();

  // Notification exists and has correct fields.
  auto* message_center = message_center::MessageCenter::Get();
  message_center::Notification* ui_notification =
      message_center->FindVisibleNotificationById("test_id");
  ASSERT_TRUE(ui_notification);
  EXPECT_EQ(55, ui_notification->progress());
  EXPECT_EQ(ASCIIToUTF16("status"), ui_notification->progress_status());

  // Update progress past 100% by creating a new notification with the same ID.
  mojo_notification = mojom::Notification::New();
  mojo_notification->type = mojom::NotificationType::kProgress;
  mojo_notification->id = "test_id";
  mojo_notification->progress = 101;
  mojo_notification->progress_status = ASCIIToUTF16("complete");

  MojoDelegate mojo_delegate2;
  message_center_remote_->DisplayNotification(
      std::move(mojo_notification),
      mojo_delegate2.receiver_.BindNewPipeAndPassRemote());
  message_center_remote_.FlushForTesting();

  ui_notification = message_center->FindVisibleNotificationById("test_id");
  ASSERT_TRUE(ui_notification);
  // Progress was clamped to 100.
  EXPECT_EQ(100, ui_notification->progress());
  // Status was updated.
  EXPECT_EQ(ASCIIToUTF16("complete"), ui_notification->progress_status());
}

TEST_F(MessageCenterAshTest, UserActions) {
  // Build mojo notification for display.
  auto mojo_notification = mojom::Notification::New();
  mojo_notification->type = mojom::NotificationType::kSimple;
  mojo_notification->id = "test_id";

  // Display the notification.
  MojoDelegate mojo_delegate;
  message_center_remote_->DisplayNotification(
      std::move(mojo_notification),
      mojo_delegate.receiver_.BindNewPipeAndPassRemote());
  message_center_remote_.FlushForTesting();

  // Notification exists.
  auto* message_center = message_center::MessageCenter::Get();
  message_center::Notification* ui_notification =
      message_center->FindVisibleNotificationById("test_id");
  ASSERT_TRUE(ui_notification);

  // Simulate the user clicking on the notification body.
  ui_notification->delegate()->Click(/*button_index=*/base::nullopt,
                                     /*reply=*/base::nullopt);
  mojo_delegate.receiver_.FlushForTesting();
  EXPECT_EQ(1, mojo_delegate.clicked_count_);

  // Simulate the user clicking on a notification button.
  ui_notification->delegate()->Click(/*button_index=*/1,
                                     /*reply=*/base::nullopt);
  mojo_delegate.receiver_.FlushForTesting();
  EXPECT_EQ(1, mojo_delegate.button_clicked_count_);
  EXPECT_EQ(1u, mojo_delegate.last_button_index_);

  // Simulate the user clicking on the settings button.
  ui_notification->delegate()->SettingsClick();
  mojo_delegate.receiver_.FlushForTesting();
  EXPECT_EQ(1, mojo_delegate.settings_button_clicked_count_);

  // Simulate the user disabling this notification.
  ui_notification->delegate()->DisableNotification();
  mojo_delegate.receiver_.FlushForTesting();
  EXPECT_EQ(1, mojo_delegate.disabled_count_);

  // Close the notification.
  message_center_remote_->CloseNotification("test_id");
  message_center_remote_.FlushForTesting();
  EXPECT_FALSE(message_center->FindVisibleNotificationById("test_id"));
  EXPECT_EQ(1, mojo_delegate.closed_count_);
}

}  // namespace
}  // namespace crosapi
