// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/nearby_share_overlay_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

namespace arc {

class NearbyShareOverlayViewTest : public views::ViewsTestBase {
 public:
  std::unique_ptr<NearbyShareOverlayView> CreateTestOverlayView(
      std::unique_ptr<views::View> dialog_view) {
    return base::WrapUnique(new NearbyShareOverlayView(dialog_view.get()));
  }
};

TEST_F(NearbyShareOverlayViewTest, ShowAndClose) {
  std::vector<std::unique_ptr<views::View>> dialog_views;
  dialog_views.push_back(nullptr);
  dialog_views.push_back(std::make_unique<views::View>());

  for (auto& dialog_view : dialog_views) {
    const bool has_dialog_view = !!dialog_view;

    auto widget =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget->SetContentsView(CreateTestOverlayView(std::move(dialog_view)));

    const auto& view_ax = widget->GetRootView()->GetViewAccessibility();
    EXPECT_EQ(!has_dialog_view, view_ax.GetIsIgnored());

    widget->Show();
    EXPECT_TRUE(widget->IsVisible());

    widget->Close();
    EXPECT_TRUE(widget->IsClosed());
  }
}

}  // namespace arc
