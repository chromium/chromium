// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/refresh_banner_view.h"

#include <memory>
#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/mahi/mahi_constants.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class RefreshBannerViewTest : public views::ViewsTestBase {
 public:
  RefreshBannerViewTest() {
    scoped_manager_setter_ =
        std::make_unique<chromeos::ScopedMahiManagerSetter>(
            &fake_mahi_manager_);
  }
  RefreshBannerViewTest(const RefreshBannerViewTest&) = delete;
  RefreshBannerViewTest& operator=(const RefreshBannerViewTest&) = delete;
  ~RefreshBannerViewTest() override = default;

  void SetContentTitle(const std::u16string& content_title) {
    fake_mahi_manager_.set_content_title(content_title);
  }

 private:
  FakeMahiManager fake_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_manager_setter_;
};

TEST_F(RefreshBannerViewTest, ShowsCorrectTitle) {
  RefreshBannerView banner_view;

  const std::u16string kContentTitle = u"New content";
  SetContentTitle(kContentTitle);
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
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  RefreshBannerView* banner_view =
      widget->SetContentsView(std::make_unique<RefreshBannerView>());

  banner_view->Show();
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
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  RefreshBannerView* banner_view =
      widget->SetContentsView(std::make_unique<RefreshBannerView>());

  // Call `Show` then `Hide` in succession. The banner should not be visible
  // after animations finish.
  banner_view->Show();
  banner_view->Hide();
  EXPECT_TRUE(banner_view->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter().Wait(banner_view->layer());
  EXPECT_FALSE(banner_view->GetVisible());
}

TEST_F(RefreshBannerViewTest, ShowImmediatelyAfterHide) {
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  RefreshBannerView* banner_view =
      widget->SetContentsView(std::make_unique<RefreshBannerView>());

  // Ensure the banner is initially visible, so that `Hide` will trigger an
  // animation.
  banner_view->Show();
  ui::LayerAnimationStoppedWaiter().Wait(banner_view->layer());
  EXPECT_TRUE(banner_view->GetVisible());

  // Call `Hide` then `Show` in succession. The banner should be visible after
  // animations finish.
  banner_view->Hide();
  banner_view->Show();
  EXPECT_TRUE(banner_view->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter().Wait(banner_view->layer());
  EXPECT_TRUE(banner_view->GetVisible());
}

}  // namespace
}  // namespace ash
