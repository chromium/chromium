// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"

// Represents the bubble top border offset, with respect to the
// Picture-in-Picture window tittle bar. Used to allow the Bubble to overlap the
// title bar.
constexpr int kBubbleTopOffset = -2;

// Used to set the control view buttons corner radious.
constexpr int kControlViewButtonCornerRadius = 20;

// Control view buttons width and height.
constexpr int kControlViewButtonWidth = 280;
constexpr int kControlViewButtonHeight = 36;

AutoPipSettingView::AutoPipSettingView(
    ResultCb result_cb,
    base::OnceCallback<void()> hide_view_cb,
    const gfx::Rect& browser_view_overridden_bounds,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    gfx::NativeView parent)
    : views::BubbleDialogDelegateView(anchor_view, arrow),
      result_cb_(std::move(result_cb)),
      hide_view_cb_(std::move(hide_view_cb)) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  DCHECK(parent);
  CHECK(result_cb_);
  set_parent_window(parent);
  SetAnchorView(anchor_view);
  InitBubble();
}

AutoPipSettingView::~AutoPipSettingView() {
  autopip_description_ = nullptr;
  allow_once_button_ = allow_on_every_visit_button_ = block_button_ = nullptr;
}

void AutoPipSettingView::InitBubble() {}

raw_ptr<views::MdTextButton> AutoPipSettingView::InitControlViewButton(
    views::BoxLayoutView* controls_view,
    UiResult ui_result,
    const std::u16string& label_text) {
  auto* button =
      controls_view->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&AutoPipSettingView::OnButtonPressed,
                              base::Unretained(this), ui_result),
          // TODO(crbug.com/1465529): Localize this.
          label_text));
  button->SetStyle(ui::ButtonStyle::kTonal);
  button->SetCornerRadius(kControlViewButtonCornerRadius);
  button->SetMinSize(
      gfx::Size(kControlViewButtonWidth, kControlViewButtonHeight));
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  return button;
}

void AutoPipSettingView::Show() {
  auto* widget = BubbleDialogDelegateView::CreateBubble(base::WrapUnique(this));
  widget->Show();
}

void AutoPipSettingView::SetDialogTitle(const std::u16string& text) {
  SetTitle(text);
  OnAnchorBoundsChanged();
}

void AutoPipSettingView::OnButtonPressed(UiResult result) {
  CHECK(result_cb_);

  std::move(result_cb_).Run(result);

  // Close the widget.
  GetWidget()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// views::BubbleDialogDelegateView:
gfx::Rect AutoPipSettingView::GetAnchorRect() const {
  const auto anchor_rect = BubbleDialogDelegateView::GetAnchorRect();
  const auto old_origin = anchor_rect.origin();
  const auto old_size = anchor_rect.size();
  const auto new_anchor_rect =
      gfx::Rect(old_origin.x(), old_origin.y() + kBubbleTopOffset,
                old_size.width(), old_size.height());
  return new_anchor_rect;
}
