// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/overlay_dialog.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

namespace arc {

class OverlayDialogTest : public views::ViewsTestBase {
 public:
  std::unique_ptr<OverlayDialog> CreateTestOverlayDialog(
      base::OnceClosure closing_callback,
      std::unique_ptr<views::View> dialog_view) {
    return base::WrapUnique(
        new OverlayDialog(std::move(closing_callback), std::move(dialog_view)));
  }
};

TEST_F(OverlayDialogTest, ShowAndClose) {
  std::vector<std::unique_ptr<views::View>> dialog_views;
  dialog_views.push_back(nullptr);
  dialog_views.push_back(std::make_unique<views::View>());

  for (auto& dialog_view : dialog_views) {
    const bool has_dialog_view = !!dialog_view;

    auto widget =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    bool called = false;
    widget->SetContentsView(CreateTestOverlayDialog(
        base::BindLambdaForTesting([&]() { called = true; }),
        std::move(dialog_view)));

    const auto& view_ax = widget->GetRootView()->GetViewAccessibility();
    EXPECT_EQ(!has_dialog_view, view_ax.GetIsIgnored());

    widget->Show();
    EXPECT_FALSE(called);

    widget.reset();
    EXPECT_TRUE(called);
  }
}

}  // namespace arc
