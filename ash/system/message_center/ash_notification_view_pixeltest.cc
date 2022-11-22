// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr char kShortTitleString[] = "Short Title";
constexpr char kMediumTitleString[] = "Test Notification's Multiline Title";
constexpr char kLongTitleString[] =
    "Test Notification's Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Very Very Very Very Very Very Very Very Very Very "
    "Very Very Very Very Long Multiline Title";

constexpr char kShortTitleScreenshot[] = "ash_notification_short_title.rev_1";
constexpr char kMediumTitleScreenshot[] =
    "ash_notification_multiline_medium_title.rev_1";
constexpr char kLongTitleScreenshot[] =
    "ash_notification_multiline_long_title.rev_1";

}  // namespace

// Pixel tests for Chrome OS Notification views.
class AshNotificationViewTitlePixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::pair<const char* /*notification title string*/,
                    const char* /*screenshot name*/>> {
 public:
  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // The `NotificationCenterTray` does not exist until the `QsRevamp` feature
    // is enabled.
    test_api_ = std::make_unique<NotificationCenterTestApi>(
        /*notification_center_tray=*/nullptr);
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

 private:
  std::unique_ptr<NotificationCenterTestApi> test_api_;
};

INSTANTIATE_TEST_SUITE_P(
    TitleTest,
    AshNotificationViewTitlePixelTest,
    testing::ValuesIn({
        std::make_pair(kShortTitleString, kShortTitleScreenshot),
        std::make_pair(kMediumTitleString, kMediumTitleScreenshot),
        std::make_pair(kLongTitleString, kLongTitleScreenshot),
    }));

// Regression test for b/251686063. Tests that a notification with a medium
// length multiline title and an icon is correctly displayed. This string would
// not be displayed properly without the workaround implemented for b/251686063.
TEST_P(AshNotificationViewTitlePixelTest, NotificationTitleTest) {
  // Create a notification with a multiline title and an icon.
  const std::string title = GetParam().first;

  const std::string id = test_api()->AddCustomNotification(
      title, "Notification Content",
      ui::ImageModel::FromImageSkia(CreateSolidColorTestImage(
          gfx::Size(/*width=*/45, /*height=*/45), SK_ColorGREEN)));

  test_api()->ToggleBubble();

  // Make sure the notification view exists and is visible.
  views::View* notification_view = test_api()->GetNotificationViewForId(id);
  ASSERT_TRUE(notification_view);
  EXPECT_TRUE(notification_view->GetVisible());

  // Compare pixels.
  const std::string screenshot = GetParam().second;
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      screenshot, notification_view));
}

}  // namespace ash
