// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"

AutoPipSettingOverlayView::AutoPipSettingOverlayView(ResultCb result_cb)
    : result_cb_(std::move(result_cb)) {
  CHECK(result_cb_);
  // Create the content setting UI.
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  // Add the semi-opaque background layer.
  auto* background =
      AddChildView(views::Builder<views::View>()
                       .SetPaintToLayer()
                       .SetBackground(views::CreateThemedSolidBackground(
                           kColorPipWindowBackground))
                       .Build());
  background->layer()->SetOpacity(0.7f);

  // Add the buttons.
  // TODO(crbug.com/1465529): Make this look like the mocks.
  auto* controls_view = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetInsideBorderInsets(gfx::Insets::TLBR(20, 20, 30, 20))
          .SetBetweenChildSpacing(30)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetPaintToLayer(ui::LAYER_NOT_DRAWN)
          .Build());

  allow_button_ =
      controls_view->AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(&AutoPipSettingOverlayView::OnButtonPressed,
                              base::Unretained(this), UiResult::kAllow),
          // TODO(crbug.com/1465529): Localize this.
          u"Allow"));
  allow_button_->SetBackground(views::CreateThemedSolidBackground(
      kColorPipWindowSkipAdButtonBackground));
  allow_button_->SetPaintToLayer();

  block_button_ =
      controls_view->AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(&AutoPipSettingOverlayView::OnButtonPressed,
                              base::Unretained(this), UiResult::kBlock),
          // TODO(crbug.com/1465529): Localize this.
          u"Block"));
  block_button_->SetBackground(views::CreateThemedSolidBackground(
      kColorPipWindowHangUpButtonForeground));
  block_button_->SetPaintToLayer();
}

AutoPipSettingOverlayView::~AutoPipSettingOverlayView() {
  block_button_ = allow_button_ = nullptr;
}

void AutoPipSettingOverlayView::OnButtonPressed(UiResult result) {
  // We expect only one click.
  CHECK(result_cb_);

  // Hide the UI to prevent a second click.
  SetVisible(false);
  std::move(result_cb_).Run(result);
}
