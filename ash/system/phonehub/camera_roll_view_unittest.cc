// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_view.h"

#include "ash/system/phonehub/camera_roll_thumbnail.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/components/phonehub/camera_roll_item.h"
#include "chromeos/ash/components/phonehub/fake_camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/fake_user_action_recorder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"

namespace ash {

class CameraRollViewForTest : public CameraRollView {
 public:
  CameraRollViewForTest(phonehub::CameraRollManager* camera_roll_manager,
                        phonehub::UserActionRecorder* user_action_recorder)
      : CameraRollView(camera_roll_manager, user_action_recorder) {}
  ~CameraRollViewForTest() override = default;
};

class CameraRollViewTest : public AshTestBase {
 public:
  CameraRollViewTest() = default;
  ~CameraRollViewTest() override = default;

  // AshTestBase:
  void TearDown() override {
    camera_roll_view_.reset();
    fake_camera_roll_manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  const CameraRollView* camera_roll_view() const {
    return camera_roll_view_.get();
  }

  phonehub::FakeCameraRollManager* fake_camera_roll_manager() {
    return fake_camera_roll_manager_.get();
  }

  void PresetCameraRollOptInState(bool can_be_enabled) {
    fake_camera_roll_manager_ =
        std::make_unique<phonehub::FakeCameraRollManager>();
    fake_camera_roll_manager_->SetIsCameraRollAvailableToBeEnabled(
        can_be_enabled);
    fake_user_action_recorder_ =
        std::make_unique<phonehub::FakeUserActionRecorder>();
    camera_roll_view_ = std::make_unique<CameraRollViewForTest>(
        fake_camera_roll_manager_.get(), fake_user_action_recorder_.get());
  }

  const std::vector<phonehub::CameraRollItem> CreateFakeItems(int num) {
    std::vector<phonehub::CameraRollItem> items;
    for (int i = num; i > 0; --i) {
      phonehub::proto::CameraRollItemMetadata metadata;
      metadata.set_key(base::NumberToString(i));
      metadata.set_mime_type("image/jpeg");
      metadata.set_last_modified_millis(1577865600 + i);
      metadata.set_file_size_bytes(123456);
      metadata.set_file_name("fake_file_" + base::NumberToString(i) + ".jpg");

      SkBitmap bitmap;
      bitmap.allocN32Pixels(96, 96);
      gfx::Image thumbnail = gfx::Image::CreateFrom1xBitmap(bitmap);

      items.emplace_back(metadata, thumbnail);
    }
    return items;
  }

  const std::vector<phonehub::CameraRollItem> CreateSingleItemWithType(
      bool is_video) {
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

    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    gfx::Image thumbnail = gfx::Image::CreateFrom1xBitmap(bitmap);

    return std::vector<phonehub::CameraRollItem>{
        phonehub::CameraRollItem(metadata, thumbnail)};
  }

  CameraRollView::CameraRollItemsView* GetItemsView() const {
    return static_cast<CameraRollView::CameraRollItemsView*>(
        camera_roll_view()->children().at(1));
  }

  CameraRollThumbnail* GetThumbnailView(int index) const {
    return static_cast<CameraRollThumbnail*>(
        GetItemsView()->children().at(index));
  }

 private:
  std::unique_ptr<CameraRollView> camera_roll_view_;
  std::unique_ptr<phonehub::FakeUserActionRecorder> fake_user_action_recorder_;
  std::unique_ptr<phonehub::FakeCameraRollManager> fake_camera_roll_manager_;
};

TEST_F(CameraRollViewTest, OptInAlready) {
  PresetCameraRollOptInState(/*can_be_enabled=*/false);

  fake_camera_roll_manager()->ClearCurrentItems();
  EXPECT_FALSE(camera_roll_view()->GetVisible());

  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(1));
  EXPECT_TRUE(camera_roll_view()->GetVisible());
  EXPECT_TRUE(camera_roll_view()->items_view_->GetVisible());
}

TEST_F(CameraRollViewTest, OptInAndDismissed) {
  PresetCameraRollOptInState(/*can_be_enabled=*/true);

  fake_camera_roll_manager()->ClearCurrentItems();
  EXPECT_FALSE(camera_roll_view()->GetVisible());
  // Setting disabled, shouldn't display anything even if there are camera roll
  // items
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(1));
  EXPECT_FALSE(camera_roll_view()->GetVisible());
}

TEST_F(CameraRollViewTest, ViewVisibility) {
  PresetCameraRollOptInState(/*can_be_enabled=*/false);
  // The camera roll view is not visible if there are no items available and
  // visible when there are one or more items available.
  fake_camera_roll_manager()->ClearCurrentItems();
  EXPECT_FALSE(camera_roll_view()->GetVisible());

  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(1));
  EXPECT_TRUE(camera_roll_view()->GetVisible());

  fake_camera_roll_manager()->ClearCurrentItems();
  EXPECT_FALSE(camera_roll_view()->GetVisible());
}

TEST_F(CameraRollViewTest, SingleItem) {
  PresetCameraRollOptInState(/*can_be_enabled=*/false);
  // Set 1 camera roll item.
  size_t expected_size = 1;
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(expected_size));
  EXPECT_EQ(GetItemsView()->children().size(), expected_size);
}

TEST_F(CameraRollViewTest, MultipleItems) {
  PresetCameraRollOptInState(/*can_be_enabled=*/false);
  // Set 4 camera roll items.
  size_t expected_size = 4;
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(expected_size));
  EXPECT_EQ(GetItemsView()->children().size(), expected_size);
}

TEST_F(CameraRollViewTest, ViewLayout) {
  PresetCameraRollOptInState(/*can_be_enabled=*/false);
  // Test the layout size and positions of the items. If the layout is being
  // intentionally changed this test will need to be updated.
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(4));
  views::test::RunScheduledLayout(GetItemsView());
  EXPECT_EQ(GetItemsView()->CalculatePreferredSize({}), gfx::Size(328, 82));
  EXPECT_EQ(GetThumbnailView(0)->bounds(), gfx::Rect(4, 4, 74, 74));
  EXPECT_EQ(GetThumbnailView(1)->bounds(), gfx::Rect(86, 4, 74, 74));
  EXPECT_EQ(GetThumbnailView(2)->bounds(), gfx::Rect(168, 4, 74, 74));
  EXPECT_EQ(GetThumbnailView(3)->bounds(), gfx::Rect(250, 4, 74, 74));
}

TEST_F(CameraRollViewTest, AccessibleNameAndTooltip) {
  PresetCameraRollOptInState(/*can_be_enabled=*/false);
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(4));

  EXPECT_EQ(u"Recent photo 1 of 4.",
            GetThumbnailView(0)->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(u"Recent photo 1 of 4.", GetThumbnailView(0)->GetTooltipText());
  EXPECT_EQ(u"Recent photo 2 of 4.",
            GetThumbnailView(1)->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(u"Recent photo 2 of 4.", GetThumbnailView(1)->GetTooltipText());
  EXPECT_EQ(u"Recent photo 3 of 4.",
            GetThumbnailView(2)->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(u"Recent photo 3 of 4.", GetThumbnailView(2)->GetTooltipText());
  EXPECT_EQ(u"Recent photo 4 of 4.",
            GetThumbnailView(3)->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(u"Recent photo 4 of 4.", GetThumbnailView(3)->GetTooltipText());
}

TEST_F(CameraRollViewTest, ImageThumbnail) {
  PresetCameraRollOptInState(/*can_be_enabled=*/false);
  fake_camera_roll_manager()->SetCurrentItems(
      CreateSingleItemWithType(/*is_video=*/false));

  EXPECT_EQ(GetThumbnailView(0)->GetMediaType(),
            phone_hub_metrics::CameraRollMediaType::kPhoto);
}

TEST_F(CameraRollViewTest, VideoThumbnail) {
  PresetCameraRollOptInState(/*can_be_enabled=*/false);
  fake_camera_roll_manager()->SetCurrentItems(
      CreateSingleItemWithType(/*is_video=*/true));

  EXPECT_EQ(GetThumbnailView(0)->GetMediaType(),
            phone_hub_metrics::CameraRollMediaType::kVideo);
}

}  // namespace ash
