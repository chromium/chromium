// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_widget.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeMahiManager> fake_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
};

TEST_F(MahiPanelWidgetTest, WidgetBounds) {
  auto* root_window = GetContext();
  auto widget = MahiPanelWidget::CreatePanelWidget(GetPrimaryDisplay().id());

  auto bottom_right = root_window->bounds().bottom_right();
  EXPECT_EQ(
      gfx::Rect(
          bottom_right.x() - kPanelDefaultWidth - kPanelBoundsShelfPadding,
          bottom_right.y() - kPanelDefaultHeight -
              ShelfConfig::Get()->shelf_size() - kPanelBoundsShelfPadding,
          kPanelDefaultWidth, kPanelDefaultHeight),
      widget->GetRestoredBounds());
}

TEST_F(MahiPanelWidgetTest, WidgetBoundsWithRefreshBanner) {
  auto widget = MahiPanelWidget::CreatePanelWidget(GetPrimaryDisplay().id());

  auto* panel_view = widget->GetContentsView()->GetViewByID(
      mahi_constants::ViewId::kMahiPanelView);

  auto* refresh_view = widget->GetContentsView()->GetViewByID(
      mahi_constants::ViewId::kRefreshView);

  auto panel_view_bounds = panel_view->GetBoundsInScreen();
  auto widget_bounds = widget->GetRestoredBounds();

  // Make sure the panel takes up the entire available space in the widget when
  // `refresh_view` is not shown.
  EXPECT_EQ(panel_view_bounds, widget_bounds);

  refresh_view->SetVisible(true);

  // Make sure the `MahiPanelView` has the exact same location on the screen
  // after the `RefreshBannerView` changes visibility.
  EXPECT_EQ(panel_view_bounds, panel_view->GetBoundsInScreen());

  // The widget's height should increase by the height of the
  // `RefreshBannerView` subtracted by `kRefreshBannerStackDepth`.
  int height_delta =
      widget->GetRestoredBounds().height() - widget_bounds.height();
  EXPECT_EQ(height_delta, refresh_view->GetBoundsInScreen().height() -
                              mahi_constants::kRefreshBannerStackDepth);
}

}  // namespace ash
