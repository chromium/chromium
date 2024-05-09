// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_widget.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kPanelDefaultWidth = 360;
constexpr int kPanelDefaultHeight = 492;
constexpr int kPanelBoundsShelfPadding = 8;

}  // namespace

class MahiPanelWidgetTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kMahi);
    AshTestBase::SetUp();

    fake_mahi_manager_ = std::make_unique<FakeMahiManager>();
    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        fake_mahi_manager_.get());
  }

  void TearDown() override {
    scoped_setter_.reset();
    fake_mahi_manager_.reset();

    AshTestBase::TearDown();
  }

 protected:
  MahiUiController ui_controller_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeMahiManager> fake_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
};

TEST_F(MahiPanelWidgetTest, DefaultWidgetBounds) {
  auto* root_window = GetContext();
  auto widget = MahiPanelWidget::CreatePanelWidget(GetPrimaryDisplay().id(),
                                                   &ui_controller_);

  auto bottom_right = root_window->bounds().bottom_right();
  EXPECT_EQ(
      gfx::Rect(
          bottom_right.x() - kPanelDefaultWidth - kPanelBoundsShelfPadding,
          bottom_right.y() - kPanelDefaultHeight -
              ShelfConfig::Get()->shelf_size() - kPanelBoundsShelfPadding,
          kPanelDefaultWidth, kPanelDefaultHeight),
      widget->GetRestoredBounds());
}

TEST_F(MahiPanelWidgetTest, WidgetBoundsAfterRefreshBannerUpdate) {
  views::UniqueWidgetPtr panel_widget = MahiPanelWidget::CreatePanelWidget(
      GetPrimaryDisplay().id(), &ui_controller_);
  // Set the widget bounds to be different to the default bounds, so that we can
  // test that the panel location is preserved.
  panel_widget->SetBounds(gfx::Rect(100, 200, 300, 200));
  const gfx::Rect kInitialPanelWidgetBounds =
      panel_widget->GetWindowBoundsInScreen();
  views::View* panel_view = panel_widget->GetContentsView()->GetViewByID(
      mahi_constants::ViewId::kMahiPanelView);
  const gfx::Rect kInitialPanelViewBounds = panel_view->GetBoundsInScreen();

  views::View* refresh_view = panel_widget->GetContentsView()->GetViewByID(
      mahi_constants::ViewId::kRefreshView);
  refresh_view->SetVisible(true);

  // The widget bounds should now provide space for the refresh banner at the
  // top, while preserving the panel view bounds.
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen().InsetsFrom(
                kInitialPanelWidgetBounds),
            gfx::Insets::TLBR(refresh_view->height() -
                                  mahi_constants::kRefreshBannerStackDepth,
                              0, 0, 0));
  EXPECT_EQ(panel_view->GetBoundsInScreen(), kInitialPanelViewBounds);

  refresh_view->SetVisible(false);

  // The panel widget and view bounds should be restored to their values before
  // the refresh banner was shown.
  EXPECT_EQ(panel_widget->GetWindowBoundsInScreen(), kInitialPanelWidgetBounds);
  EXPECT_EQ(panel_view->GetBoundsInScreen(), kInitialPanelViewBounds);
}

}  // namespace ash
