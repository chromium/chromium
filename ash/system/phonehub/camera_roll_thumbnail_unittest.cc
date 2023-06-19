// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_thumbnail.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/logging.h"
#include "chromeos/ash/components/phonehub/camera_roll_item.h"
#include "chromeos/ash/components/phonehub/fake_camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {
constexpr int kRectWidthInDip = 120;
constexpr int kRectHeightInDip = 70;
constexpr gfx::Size kExpectedCameraRollThumbnailBorderSize(74, 74);
constexpr gfx::Point kExpectedCameraRollThumbnailVideoCircleOrigin(37, 37);
constexpr int kExpectedCameraRollThumbnailVideoCircleRadius = 16;
constexpr gfx::Point kExpectedCameraRollThumbnailVideoIconOrigin(27, 27);
constexpr int kExpectedCameraRollThumbnailVideoIconSize = 20;

class CameraRollThumbnailForTest : public CameraRollThumbnail {
 public:
  CameraRollThumbnailForTest(const phonehub::CameraRollItem& test_item,
                             phonehub::CameraRollManager* camera_roll_manager,
                             phonehub::UserActionRecorder* user_action_recorder)
      : CameraRollThumbnail(1,
                            test_item,
                            camera_roll_manager,
                            user_action_recorder) {}
  ~CameraRollThumbnailForTest() override = default;
};

class CameraRollThumbnailTest : public views::ViewsTestBase {
 public:
  CameraRollThumbnailTest() = default;
  ~CameraRollThumbnailTest() override = default;

  // ViewTestBase:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    fake_camera_roll_manager_ =
        std::make_unique<phonehub::FakeCameraRollManager>();
    fake_user_action_recorder_ =
        std::make_unique<phonehub::FakeUserActionRecorder>();
    CreateWidget();
    generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_));
  }

  void TearDown() override {
    camera_roll_thumbnail_.reset();
    fake_camera_roll_manager_.reset();
    fake_user_action_recorder_.reset();
    generator_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  views::Widget* widget() { return widget_; }
  ui::test::EventGenerator* generator() { return generator_.get(); }

  CameraRollThumbnail* camera_roll_thumbnail() const {
    return camera_roll_thumbnail_.get();
  }

  phonehub::FakeCameraRollManager* fake_camera_roll_manager() {
    return fake_camera_roll_manager_.get();
  }

  void CreateWidget() {
    DCHECK(!widget_);
    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));
  }

  void SetUpCameraRollThumbnailForTest(bool is_video) {
    auto thumbnail_button = std::make_unique<CameraRollThumbnailForTest>(
        CreateCameraRollItemWithType(is_video), fake_camera_roll_manager_.get(),
        fake_user_action_recorder_.get());
    camera_roll_thumbnail_ = std::unique_ptr<CameraRollThumbnailForTest>(
        widget_->SetContentsView(std::move(thumbnail_button)));
    camera_roll_thumbnail_->SetBoundsRect(
        gfx::Rect(0, 0, kRectWidthInDip, kRectHeightInDip));
    widget_->Show();
  }

  SkBitmap CreateExpectedThumbnail(bool is_video) {
    gfx::Canvas expected(gfx::Size(kRectWidthInDip, kRectHeightInDip),
                         /*image_scale=*/1.0f, /*is_opaque=*/true);
    auto* provider = AshColorProvider::Get();
    expected.DrawColor(provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));

    gfx::ImageSkia test_image = CreateTestThumbnail().AsImageSkia();
    expected.DrawImageInt(
        test_image, 0, 0, test_image.width(), test_image.height(), 0, 0,
        kExpectedCameraRollThumbnailBorderSize.width(),
        kExpectedCameraRollThumbnailBorderSize.height(), false);

    if (is_video) {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(provider->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80));
      flags.setStyle(cc::PaintFlags::kFill_Style);
      expected.DrawCircle(kExpectedCameraRollThumbnailVideoCircleOrigin,
                          kExpectedCameraRollThumbnailVideoCircleRadius, flags);
      expected.DrawImageInt(
          CreateVectorIcon(
              kPhoneHubCameraRollItemVideoIcon,
              kExpectedCameraRollThumbnailVideoIconSize,
              provider->GetContentLayerColor(
                  AshColorProvider::ContentLayerType::kIconColorPrimary)),
          kExpectedCameraRollThumbnailVideoIconOrigin.x(),
          kExpectedCameraRollThumbnailVideoIconOrigin.y());
    }

    return expected.GetBitmap();
  }

 private:
  const phonehub::CameraRollItem CreateCameraRollItemWithType(bool is_video) {
    phonehub::proto::CameraRollItemMetadata metadata;
    metadata.set_key("key");
    metadata.set_last_modified_millis(1577865600);
    metadata.set_file_size_bytes(123456);

    if (is_video) {
      metadata.set_mime_type("video/mp4");
      metadata.set_file_name("fake_video.mp4");
    } else {
      metadata.set_mime_type("image/png");
      metadata.set_file_name("fake_image.png");
    }

    return phonehub::CameraRollItem(metadata, CreateTestThumbnail());
  }

  const gfx::Image CreateTestThumbnail() {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    return gfx::Image::CreateFrom1xBitmap(bitmap);
  }

  // This is required in order for the context to find color provider
  AshColorProvider color_provider_;
  raw_ptr<views::Widget, DanglingUntriaged> widget_ = nullptr;
  std::unique_ptr<CameraRollThumbnail> camera_roll_thumbnail_;
  std::unique_ptr<phonehub::FakeUserActionRecorder> fake_user_action_recorder_;
  std::unique_ptr<phonehub::FakeCameraRollManager> fake_camera_roll_manager_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

TEST_F(CameraRollThumbnailTest, ViewLayout) {
  SetUpCameraRollThumbnailForTest(/*is_video=*/false);

  EXPECT_EQ(camera_roll_thumbnail()->GetFocusBehavior(),
            CameraRollThumbnail::FocusBehavior::ALWAYS);
  EXPECT_STREQ("CameraRollThumbnail", camera_roll_thumbnail()->GetClassName());
}

TEST_F(CameraRollThumbnailTest, ImageThumbnail) {
  SetUpCameraRollThumbnailForTest(/*is_video=*/false);
  EXPECT_EQ(camera_roll_thumbnail()->GetFocusBehavior(),
            CameraRollThumbnail::FocusBehavior::ALWAYS);

  gfx::Canvas placeholder(gfx::Size(kRectWidthInDip, kRectHeightInDip),
                          /*image_scale=*/1.0f, /*is_opaque=*/true);
  gfx::Canvas* ptr_placeholder;
  ptr_placeholder = &placeholder;
  camera_roll_thumbnail()->PaintButtonContents(ptr_placeholder);

  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(CreateExpectedThumbnail(/*is_video=*/false),
                                 ptr_placeholder->GetBitmap()));
}

TEST_F(CameraRollThumbnailTest, VideoThumbnail) {
  SetUpCameraRollThumbnailForTest(/*is_video=*/true);
  EXPECT_EQ(camera_roll_thumbnail()->GetFocusBehavior(),
            CameraRollThumbnail::FocusBehavior::ALWAYS);

  gfx::Canvas placeholder(gfx::Size(kRectWidthInDip, kRectHeightInDip),
                          /*image_scale=*/1.0f, /*is_opaque=*/true);
  gfx::Canvas* ptr_placeholder;
  ptr_placeholder = &placeholder;
  camera_roll_thumbnail()->PaintButtonContents(ptr_placeholder);

  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(CreateExpectedThumbnail(/*is_video=*/true),
                                 ptr_placeholder->GetBitmap()));
}

TEST_F(CameraRollThumbnailTest, LeftClickDownload) {
  SetUpCameraRollThumbnailForTest(/*is_video=*/false);
  EXPECT_TRUE(!camera_roll_thumbnail()->menu_model_);

  // Left click button
  generator()->MoveMouseTo(
      camera_roll_thumbnail()->GetBoundsInScreen().CenterPoint());
  generator()->ClickLeftButton();

  // Menu model of type CameraRollMenuModel is not created
  EXPECT_FALSE(dynamic_cast<CameraRollMenuModel*>(
                   camera_roll_thumbnail()->menu_model_.get()) != nullptr);

  // Download was triggered
  EXPECT_EQ(fake_camera_roll_manager()->GetDownloadRequestCount(), 1);
}

TEST_F(CameraRollThumbnailTest, RightClickOpenMenu) {
  SetUpCameraRollThumbnailForTest(/*is_video=*/false);
  EXPECT_TRUE(!camera_roll_thumbnail()->menu_model_);

  // Right click button
  generator()->MoveMouseTo(
      camera_roll_thumbnail()->GetBoundsInScreen().CenterPoint());
  generator()->ClickRightButton();

  // Download was not triggered
  EXPECT_EQ(fake_camera_roll_manager()->GetDownloadRequestCount(), 0);

  // Menu model of type CameraRollMenuModel is created
  EXPECT_TRUE(dynamic_cast<CameraRollMenuModel*>(
                  camera_roll_thumbnail()->menu_model_.get()) != nullptr);

  // Triggering menu item callback
  camera_roll_thumbnail()->menu_model_.get()->ExecuteCommand(
      CameraRollMenuModel::CommandID::COMMAND_DOWNLOAD, 0);

  // Download was triggered
  EXPECT_EQ(fake_camera_roll_manager()->GetDownloadRequestCount(), 1);
}
}  // namespace ash
