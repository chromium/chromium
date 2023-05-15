// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"

#include <vector>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

class BluetoothDisabledDetailedViewTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateTestWidget();
    container_ = widget_->GetContentsView()->AddChildView(
        std::make_unique<views::View>());
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    bluetooth_disabled_detailed_view_ = container_->AddChildView(
        std::make_unique<BluetoothDisabledDetailedView>());
    static_cast<views::BoxLayout*>(container_->GetLayoutManager())
        ->SetFlexForView(bluetooth_disabled_detailed_view_, 1);
    views::test::RunScheduledLayout(container_);
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  const views::ImageView* GetIcon() {
    views::View* view = bluetooth_disabled_detailed_view_->children().at(0);
    EXPECT_STREQ("ImageView", view->GetClassName());
    return static_cast<views::ImageView*>(view);
  }

  const views::Label* GetLabel() {
    views::View* view = bluetooth_disabled_detailed_view_->children().at(1);
    EXPECT_STREQ("Label", view->GetClassName());
    return static_cast<views::Label*>(view);
  }

  BluetoothDisabledDetailedView* bluetooth_disabled_detailed_view() {
    return bluetooth_disabled_detailed_view_;
  }

  views::View* container() { return container_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View, ExperimentalAsh> container_;
  raw_ptr<BluetoothDisabledDetailedView, ExperimentalAsh>
      bluetooth_disabled_detailed_view_;
};

TEST_F(BluetoothDisabledDetailedViewTest, DisabledIconIsCentered) {
  const views::ImageView* icon = GetIcon();
  const views::Label* label = GetLabel();

  // The set of arbitrary bounds for the view containing the disabled view,
  // intended to cover various layouts and offsets, used to check that the
  // icon is always centered in the view.
  const std::vector<const gfx::Rect> container_bounds{{
      // x, y, width, height
      gfx::Rect(0, 0, 100, 100),
      gfx::Rect(0, 0, 500, 100),
      gfx::Rect(0, 0, 100, 500),
      gfx::Rect(100, 0, 100, 100),
      gfx::Rect(0, 100, 100, 100),
  }};

  for (const auto& bounds : container_bounds) {
    container()->SetBoundsRect(bounds);

    const gfx::Point center_of_icon_view =
        icon->GetBoundsInScreen().CenterPoint();
    const gfx::Point center_of_disabled_panel =
        bluetooth_disabled_detailed_view()->GetBoundsInScreen().CenterPoint();

    // Padding equal to the height of the label is added to the top of the icon
    // to offset it such that the icon is centered. Add half of this padding to
    // the center of the icon view to find the center of the icon itself.
    const int padding = (label->height() + 1) / 2;

    EXPECT_EQ(center_of_disabled_panel.x(), center_of_icon_view.x());
    EXPECT_EQ(center_of_disabled_panel.y(), center_of_icon_view.y() + padding);
  }
}

}  // namespace ash
