// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"

// Represents the bubble top border offset, with respect to the
// Picture-in-Picture window tittle bar. Used to allow the Bubble to overlap the
// title bar.
constexpr int kBubbleTopOffset = -2;

// Used to set the control view buttons corner radius.
constexpr int kControlViewButtonCornerRadius = 20;

// Control view buttons width and height.
constexpr int kControlViewButtonWidth = 280;
constexpr int kControlViewButtonHeight = 36;

// Spacing between the BoxLayout children.
constexpr int kLayoutBetweenChildSpacing = 8;

// Control AutoPiP description view width and height.
constexpr int kDescriptionViewWidth = 280;
constexpr int kDescriptionViewHeight = 32;

// Short AutoPiP Description. To be displayed below the Bubble title.
// TODO(crbug.com/1465529): Localize this.
constexpr char16_t kAutopipDescription[] =
    u"Automatically enter Picture-in-Picture";

// Bubble fixed width.
constexpr int kBubbleFixedWidth = 320;

// Bubble border corner radius.
constexpr int kBubbleBorderCornerRadius = 15;

// Bubble border MD shadow elevation.
constexpr int kBubbleBorderMdShadowElevation = 3;

// Bubble margins.
constexpr gfx::Insets kBubbleMargins = gfx::Insets::TLBR(0, 15, 15, 20);

// Bubble title margins.
constexpr gfx::Insets kBubbleTitleMargins = gfx::Insets::TLBR(15, 10, 10, 10);

AutoPipSettingView::AutoPipSettingView(
    ResultCb result_cb,
    base::OnceCallback<void()> hide_view_cb,
    const gfx::Rect& browser_view_overridden_bounds,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    gfx::NativeView parent)
    : views::BubbleDialogDelegateView(anchor_view, arrow),
      result_cb_(std::move(result_cb)) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  DCHECK(parent);
  CHECK(result_cb_);
  set_parent_window(parent);
  SetAnchorView(anchor_view);
  set_fixed_width(kBubbleFixedWidth);
  // Set up callback to hide AutoPiP overlay view semi-opaque background layer.
  SetCloseCallback(std::move(hide_view_cb));
  set_fixed_width(kBubbleFixedWidth);
  set_use_custom_frame(true);
  set_margins(kBubbleMargins);
  set_title_margins(kBubbleTitleMargins);

  // Initialize Bubble.
  InitBubble();
}

AutoPipSettingView::~AutoPipSettingView() {
  autopip_description_ = nullptr;
  allow_once_button_ = allow_on_every_visit_button_ = block_button_ = nullptr;
}

void AutoPipSettingView::InitBubble() {
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout_manager->set_between_child_spacing(kLayoutBetweenChildSpacing);

  auto* description_view = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(kLayoutBetweenChildSpacing)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .Build());

  description_view->AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&autopip_description_)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetElideBehavior(gfx::NO_ELIDE)
          .SetMultiLine(true)
          .SetText(std::u16string(kAutopipDescription))
          .Build());
  autopip_description_->SetSize(
      gfx::Size(kDescriptionViewWidth, kDescriptionViewHeight));

  auto* controls_view = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(kLayoutBetweenChildSpacing)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .Build());

  // TODO(crbug.com/1465529): Localize button text labels.
  allow_once_button_ = InitControlViewButton(
      controls_view, UiResult::kAllowOnce, u"Allow this time");
  allow_on_every_visit_button_ = InitControlViewButton(
      controls_view, UiResult::kAllowOnEveryVisit, u"Allow on every visit");
  block_button_ =
      InitControlViewButton(controls_view, UiResult::kBlock, u"Don't allow");
}

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

  // Customize Bubble border.
  auto bubble_border = std::make_unique<views::BubbleBorder>(
      arrow(), views::BubbleBorder::STANDARD_SHADOW);
  bubble_border->SetCornerRadius(kBubbleBorderCornerRadius);
  bubble_border->set_md_shadow_elevation(kBubbleBorderMdShadowElevation);
  bubble_border->set_draw_border_stroke(true);
  GetBubbleFrameView()->SetBubbleBorder(std::move(bubble_border));

  widget->Show();
}

void AutoPipSettingView::SetDialogTitle(const std::u16string& text) {
  SetTitle(text);
  OnAnchorBoundsChanged();
}

void AutoPipSettingView::OnButtonPressed(UiResult result) {
  CHECK(result_cb_);

  std::move(result_cb_).Run(result);

  // Hide the View and close the widget.
  SetVisible(false);
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
