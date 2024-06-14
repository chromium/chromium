// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/refresh_banner_view.h"

#include <memory>
#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Aliases ---------------------------------------------------------------------

using ::testing::NiceMock;
using ::testing::Return;

}  // namespace

class RefreshBannerViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  RefreshBannerView* CreateBannerView() {
    auto* banner_view = widget_->SetContentsView(
        std::make_unique<RefreshBannerView>(ui_controller()));

    // Make the banner visible.
    banner_view->Show();
    return views::AsViewClass<RefreshBannerView>(banner_view);
  }

  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }

  MahiUiController* ui_controller() { return &ui_controller_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  NiceMock<MockMahiManager> mock_mahi_manager_;
  chromeos::ScopedMahiManagerSetter scoped_manager_setter_{&mock_mahi_manager_};
  MahiUiController ui_controller_;
};

TEST_F(RefreshBannerViewTest, ShowsCorrectTitle) {
  RefreshBannerView banner_view(ui_controller());

  const std::u16string kContentTitle(u"New content");
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(kContentTitle));
  banner_view.Show();

  EXPECT_EQ(
      views::AsViewClass<views::Label>(
          banner_view.GetViewByID(mahi_constants::ViewId::kBannerTitleLabel))
          ->GetText(),
      l10n_util::GetStringFUTF16(IDS_ASH_MAHI_REFRESH_BANNER_LABEL_TEXT,
                                 kContentTitle));
}

TEST_F(RefreshBannerViewTest, BannerVisibilityAnimations) {
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  RefreshBannerView* banner_view = CreateBannerView();

  EXPECT_TRUE(banner_view->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter().Wait(banner_view->layer());
  EXPECT_FALSE(banner_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(banner_view->GetVisible());

  banner_view->Hide();
  EXPECT_TRUE(banner_view->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter().Wait(banner_view->layer());
  EXPECT_FALSE(banner_view->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(banner_view->GetVisible());
}

TEST_F(RefreshBannerViewTest, HideImmediatelyAfterShow) {
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  RefreshBannerView* banner_view = CreateBannerView();

  banner_view->Hide();
  EXPECT_TRUE(banner_view->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter().Wait(banner_view->layer());
  EXPECT_FALSE(banner_view->GetVisible());
}

TEST_F(RefreshBannerViewTest, ShowImmediatelyAfterHide) {
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  RefreshBannerView* banner_view = CreateBannerView();

  ui::LayerAnimationStoppedWaiter().Wait(banner_view->layer());
  EXPECT_TRUE(banner_view->GetVisible());

  // Call `Hide` then `Show` in succession. The banner should be visible after
  // animations finish.
  banner_view->Hide();
  banner_view->Show();

  EXPECT_TRUE(banner_view->GetVisible());
}

TEST_F(RefreshBannerViewTest, RefreshingSummaryContentsHidesBanner) {
  RefreshBannerView* banner_view = CreateBannerView();

  EXPECT_TRUE(banner_view->GetVisible());

  // Triggering a summary contents refresh should hide the banner.
  ui_controller()->RefreshContents();
  EXPECT_FALSE(banner_view->GetVisible());
}

TEST_F(RefreshBannerViewTest, Metrics) {
  auto* banner_view = CreateBannerView();

  base::HistogramTester histogram;
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kRefreshButton, 0);

  LeftClickOn(banner_view->GetViewByID(mahi_constants::ViewId::kRefreshButton));

  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kRefreshButton, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 1);
}

TEST_F(RefreshBannerViewTest, ClipPathUpdatedOnVisibilityChange) {
  RefreshBannerView banner_view(ui_controller());
  ASSERT_FALSE(banner_view.GetVisible());
  banner_view.SetBounds(0, 0, 200, 200);
  // Changing the visibility should result in the clip path being updated.
  banner_view.SetClipPath(SkPath::Rect(SkRect::MakeWH(100, 100)));
  auto previous_clip_path = banner_view.clip_path();
  banner_view.SetVisible(true);
  EXPECT_NE(banner_view.clip_path(), previous_clip_path);
}

}  // namespace ash
