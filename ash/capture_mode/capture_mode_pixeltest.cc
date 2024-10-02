// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/system/notification_center/message_popup_animation_waiter.h"
#include "ash/system/notification_center/notification_center_test_api.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/message_center/views/message_popup_view.h"
#include "ui/message_center/views/message_view.h"

namespace ash {

namespace {

constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";

// The types of the primary display size.
enum class DisplayType {
  // The display size is the default one.
  kNormal,

  // The display's height is much greater than its width.
  kUltraHeight,

  // The display's width is much greater than its height.
  kUltraWidth,
};

// Returns the name of `type`.
std::string GetDisplayTypeName(DisplayType type) {
  switch (type) {
    case DisplayType::kNormal:
      return "normal";
    case DisplayType::kUltraWidth:
      return "ultra_width";
    case DisplayType::kUltraHeight:
      return "ultra_height";
  }
}

std::string GetScreenshotName(const std::string& test_name, bool new_width) {
  return test_name + (new_width ? "_new_width" : "_old_width");
}

}  // namespace

class DisplayParameterizedCaptureModePixelTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<DisplayType, bool>> {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    AshTestBase::SetUp();
    test_api_ = std::make_unique<NotificationCenterTestApi>();

    // Change the display size depending on the test param.
    switch (GetDisplayType()) {
      case DisplayType::kNormal:
        break;
      case DisplayType::kUltraWidth:
        UpdateDisplay("1200x600");
        break;
      case DisplayType::kUltraHeight:
        UpdateDisplay("600x1200");
        break;
    }

    // Create windows so that the screenshot has more contents.
    window1_ = CreateAppWindow(/*bounds_in_screen=*/gfx::Rect(200, 200));
    window2_ =
        CreateAppWindow(/*bounds_in_screen=*/gfx::Rect(220, 220, 100, 100));
    DecorateWindow(window1_.get(), u"Window1", SK_ColorDKGRAY);
    DecorateWindow(window2_.get(), u"Window2", SK_ColorBLUE);

    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(
        chromeos::features::kNotificationWidthIncrease,
        IsNotificationWidthIncreaseEnabled());
  }

  void TearDown() override {
    window2_.reset();
    window1_.reset();
    AshTestBase::TearDown();
  }

  NotificationCenterTestApi* test_api() { return test_api_.get(); }

  DisplayType GetDisplayType() const { return std::get<0>(GetParam()); }

  bool IsNotificationWidthIncreaseEnabled() { return std::get<1>(GetParam()); }

 private:
  std::unique_ptr<aura::Window> window1_;
  std::unique_ptr<aura::Window> window2_;

  std::unique_ptr<NotificationCenterTestApi> test_api_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    DisplaySize,
    DisplayParameterizedCaptureModePixelTest,
    testing::Combine(testing::ValuesIn({DisplayType::kNormal,
                                        DisplayType::kUltraWidth,
                                        DisplayType::kUltraHeight}),
                     /*IsNotificationWidthIncreaseEnabled()=*/testing::Bool()));

TEST_P(DisplayParameterizedCaptureModePixelTest,
       ScreenCaptureNotificationPopup) {
  // Take a full screenshot then wait for the file path to the saved image.
  ash::CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kImage);
  controller->PerformCapture();
  WaitForCaptureFileToBeSaved();

  // Wait until the notification popup shows.
  MessagePopupAnimationWaiter(
      GetPrimaryNotificationCenterTray()->popup_collection())
      .Wait();

  // Get the notification view.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetScreenshotName(base::StrCat({"screen_capture_popup_notification_",
                                      GetDisplayTypeName(GetDisplayType())}),
                        IsNotificationWidthIncreaseEnabled()),
      /*revision_number=*/1,
      test_api()->GetPopupViewForId(kScreenCaptureNotificationId)));
}

TEST_P(DisplayParameterizedCaptureModePixelTest, VideoCaptureNotification) {
  auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                         CaptureModeType::kVideo);
  StartVideoRecordingImmediately();
  CaptureModeTestApi().FlushRecordingServiceForTesting();

  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());

  // Request and wait for a video frame then wait for the capture notification
  // to be posted.
  test_delegate->RequestAndWaitForVideoFrame();
  CaptureNotificationWaiter waiter;
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  waiter.Wait();

  // Wait until the notification popup shows.
  MessagePopupAnimationWaiter(
      GetPrimaryNotificationCenterTray()->popup_collection())
      .Wait();

  std::string notification_id = GetPreviewNotification()->id();
  auto* notification_popup_view =
      test_api()->GetPopupViewForId(notification_id);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetScreenshotName(base::StrCat({"video_capture_notification_popup_",
                                      GetDisplayTypeName(GetDisplayType())}),
                        IsNotificationWidthIncreaseEnabled()),
      /*revision_number=*/2, notification_popup_view));

  test_api()->ToggleBubble();
  auto* notification_view =
      test_api()->GetNotificationViewForId(notification_id);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GetScreenshotName(base::StrCat({"video_capture_notification_view_",
                                      GetDisplayTypeName(GetDisplayType())}),
                        IsNotificationWidthIncreaseEnabled()),
      /*revision_number=*/2, notification_view));
}

}  // namespace ash
