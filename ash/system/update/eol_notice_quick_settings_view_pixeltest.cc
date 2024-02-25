// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/system/update/eol_notice_quick_settings_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Pixel tests for the EOL quick settings view.
class EolNoticeQuickSettingsViewPixelTest : public AshTestBase {
 public:
  EolNoticeQuickSettingsViewPixelTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    auto* contents =
        widget_->SetContentsView(std::make_unique<views::BoxLayoutView>());
    contents->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    contents->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    // The stroke color has transparency, so set a background color so it
    // renders like in production.
    contents->SetBackground(views::CreateThemedSolidBackground(
        cros_tokens::kCrosSysSystemBaseElevated));
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  std::unique_ptr<views::Widget> widget_;
};

TEST_F(EolNoticeQuickSettingsViewPixelTest, Basics) {
  auto* view = widget_->GetContentsView()->AddChildView(
      std::make_unique<EolNoticeQuickSettingsView>());
  // Use the default size from go/cros-quick-settings-spec
  view->SetPreferredSize(gfx::Size(408, 32));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view",
      /*revision_number=*/0, widget_.get()));
}

}  // namespace
}  // namespace ash
