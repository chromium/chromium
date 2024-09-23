// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/quick_settings_notice_view_pixeltest_base.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

QuickSettingsNoticeViewPixelTestBase::QuickSettingsNoticeViewPixelTestBase() =
    default;
QuickSettingsNoticeViewPixelTestBase::~QuickSettingsNoticeViewPixelTestBase() =
    default;

void QuickSettingsNoticeViewPixelTestBase::SetUp() {
  AshTestBase::SetUp();

  widget_ = CreateFramelessTestWidget();
  widget_->SetFullscreen(true);
  auto* contents =
      widget_->SetContentsView(std::make_unique<views::BoxLayoutView>());
  contents->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  contents->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  // The stroke color has transparency, so set a background color so it
  // renders like in production.
  contents->SetBackground(views::CreateThemedSolidBackground(
      cros_tokens::kCrosSysSystemBaseElevated));
}

void QuickSettingsNoticeViewPixelTestBase::TearDown() {
  widget_.reset();

  AshTestBase::TearDown();
}

std::optional<pixel_test::InitParams>
QuickSettingsNoticeViewPixelTestBase::CreatePixelTestInitParams() const {
  return pixel_test::InitParams();
}

void QuickSettingsNoticeViewPixelTestBase::DiffView(size_t revision_number) {
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "check_view", revision_number, widget_.get()));
}

}  // namespace ash
