// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_view.h"

#include "ash/components/phonehub/camera_roll_item.h"
#include "ash/components/phonehub/fake_camera_roll_manager.h"
#include "ash/components/phonehub/fake_user_action_recorder.h"
#include "ash/test/ash_test_base.h"
#include "camera_roll_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_layer_animation_delegate.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/view.h"

namespace ash {

class CameraRollViewForTest : public CameraRollView {
 public:
  CameraRollViewForTest(phonehub::CameraRollManager* camera_roll_manager,
                        phonehub::UserActionRecorder* user_action_recorder)
      : CameraRollView(camera_roll_manager, user_action_recorder) {
    should_disable_annimator_timer_for_test_ = true;
  }
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

  void PresetCameraRollOptInState(bool has_been_dismissed,
                                  bool can_be_enabled) {
    fake_camera_roll_manager_ =
        std::make_unique<phonehub::FakeCameraRollManager>();
    if (has_been_dismissed) {
      fake_camera_roll_manager_->OnCameraRollOnboardingUiDismissed();
    }
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

  const views::View* GetItemsView() const {
    return camera_roll_view()->children().at(1);
  }

  const views::MenuButton* GetThumbnailView(int index) const {
    return static_cast<views::MenuButton*>(
        GetItemsView()->children().at(index));
  }

 private:
  std::unique_ptr<CameraRollView> camera_roll_view_;
  std::unique_ptr<phonehub::FakeUserActionRecorder> fake_user_action_recorder_;
  std::unique_ptr<phonehub::FakeCameraRollManager> fake_camera_roll_manager_;
};

TEST_F(CameraRollViewTest, DisplayOptInView) {
  PresetCameraRollOptInState(/*has_been_dismissed=*/false,
                             /*can_be_enabled=*/true);

  fake_camera_roll_manager()->ClearCurrentItems();
  EXPECT_TRUE(camera_roll_view()->GetVisible());
  EXPECT_TRUE(camera_roll_view()->opt_in_view_->GetVisible());
}

TEST_F(CameraRollViewTest, OptInAlready) {
  PresetCameraRollOptInState(/*has_been_dismissed=*/false,
                             /*can_be_enabled=*/false);

  fake_camera_roll_manager()->ClearCurrentItems();
  EXPECT_FALSE(camera_roll_view()->GetVisible());

  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(1));
  EXPECT_TRUE(camera_roll_view()->GetVisible());
  EXPECT_FALSE(camera_roll_view()->opt_in_view_->GetVisible());
  EXPECT_TRUE(camera_roll_view()->items_view_->GetVisible());
}

TEST_F(CameraRollViewTest, RightAfterOptIn) {
  PresetCameraRollOptInState(/*has_been_dismissed=*/false,
                             /*can_be_enabled=*/false);
  fake_camera_roll_manager()->EnableCameraRollFeatureInSystemSetting();

  EXPECT_TRUE(camera_roll_view()->GetVisible());
  EXPECT_FALSE(camera_roll_view()->opt_in_view_->GetVisible());
  EXPECT_TRUE(camera_roll_view()->items_view_->GetVisible());
  // There should be 4 camera roll item placeholder.
  size_t expected_placeholder_seize = 4;
  EXPECT_EQ(GetItemsView()->children().size(), expected_placeholder_seize);
  size_t expected_item_size = 0;
  EXPECT_EQ(fake_camera_roll_manager()->current_items().size(),
            expected_item_size);
}

TEST_F(CameraRollViewTest, OptInAndDismissed) {
  PresetCameraRollOptInState(/*has_been_dismissed=*/true,
                             /*can_be_enabled=*/true);

  fake_camera_roll_manager()->ClearCurrentItems();
  EXPECT_FALSE(camera_roll_view()->GetVisible());
  // Setting disabled, shouldn't display anything even if there are camera roll
  // items
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(1));
  EXPECT_FALSE(camera_roll_view()->GetVisible());
}

TEST_F(CameraRollViewTest, ViewVisibility) {
  PresetCameraRollOptInState(/*has_been_dismissed=*/true,
                             /*can_be_enabled=*/false);
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
  PresetCameraRollOptInState(/*has_been_dismissed=*/true,
                             /*can_be_enabled=*/false);
  // Set 1 camera roll item.
  size_t expected_size = 1;
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(expected_size));
  EXPECT_EQ(GetItemsView()->children().size(), expected_size);
}

TEST_F(CameraRollViewTest, MultipleItems) {
  PresetCameraRollOptInState(/*has_been_dismissed=*/true,
                             /*can_be_enabled=*/false);
  // Set 4 camera roll items.
  size_t expected_size = 4;
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(expected_size));
  EXPECT_EQ(GetItemsView()->children().size(), expected_size);
}

TEST_F(CameraRollViewTest, ViewLayout) {
  PresetCameraRollOptInState(/*has_been_dismissed=*/true,
                             /*can_be_enabled=*/false);
  // Test the layout size and positions of the items. If the layout is being
  // intentionally changed this test will need to be updated.
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(4));
  EXPECT_EQ(camera_roll_view()->items_view_->CalculatePreferredSize(),
            gfx::Size(328, 82));
  EXPECT_EQ(camera_roll_view()->items_view_->GetCameraRollItemPosition(0),
            gfx::Point(4, 4));
  EXPECT_EQ(camera_roll_view()->items_view_->GetCameraRollItemPosition(1),
            gfx::Point(86, 4));
  EXPECT_EQ(camera_roll_view()->items_view_->GetCameraRollItemPosition(2),
            gfx::Point(168, 4));
  EXPECT_EQ(camera_roll_view()->items_view_->GetCameraRollItemPosition(3),
            gfx::Point(250, 4));
}

TEST_F(CameraRollViewTest, AccessibleNameAndTooltip) {
  PresetCameraRollOptInState(/*has_been_dismissed=*/true,
                             /*can_be_enabled=*/false);
  fake_camera_roll_manager()->SetCurrentItems(CreateFakeItems(4));

  EXPECT_EQ(u"Recent photo 1 of 4.", GetThumbnailView(0)->GetAccessibleName());
  EXPECT_EQ(u"Recent photo 1 of 4.", GetThumbnailView(0)->GetTooltipText());
  EXPECT_EQ(u"Recent photo 2 of 4.", GetThumbnailView(1)->GetAccessibleName());
  EXPECT_EQ(u"Recent photo 2 of 4.", GetThumbnailView(1)->GetTooltipText());
  EXPECT_EQ(u"Recent photo 3 of 4.", GetThumbnailView(2)->GetAccessibleName());
  EXPECT_EQ(u"Recent photo 3 of 4.", GetThumbnailView(2)->GetTooltipText());
  EXPECT_EQ(u"Recent photo 4 of 4.", GetThumbnailView(3)->GetAccessibleName());
  EXPECT_EQ(u"Recent photo 4 of 4.", GetThumbnailView(3)->GetTooltipText());
}

}  // namespace ash
