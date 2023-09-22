// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/views/layout/flex_layout_view.h"

// Represents the bubble top border offset, with respect to the
// Picture-in-Picture window title bar. Used to allow the Bubble to overlap the
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
    u"Enter picture-in-picture if you switch tabs on certain sites.";

// Bubble fixed width.
constexpr int kBubbleFixedWidth = 320;

// Bubble border corner radius.
constexpr int kBubbleBorderCornerRadius = 15;

// Bubble border MD shadow elevation.
constexpr int kBubbleBorderMdShadowElevation = 2;

// Bubble margins.
constexpr gfx::Insets kBubbleMargins = gfx::Insets::TLBR(0, 20, 15, 20);

// Bubble title margins.
constexpr gfx::Insets kBubbleTitleMargins = gfx::Insets::TLBR(15, 15, 10, 15);

// Bubble title, without origin.
// TODO(crbug.com/1465529): Localize this.
constexpr char16_t kBubbleTitleSuffix[] = u" wants to";

// Maximum width for the origin label, for cases where the origin needs to be
// elided.
constexpr int kBubbleOriginLabelMaximumWidth = 230;

AutoPipSettingView::AutoPipSettingView(
    ResultCb result_cb,
    HideViewCb hide_view_cb,
    const GURL& origin,
    const gfx::Rect& browser_view_overridden_bounds,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow)
    : views::BubbleDialogDelegate(anchor_view, arrow),
      result_cb_(std::move(result_cb)) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  CHECK(result_cb_);
  SetAnchorView(anchor_view);
  set_fixed_width(kBubbleFixedWidth);

  // Set up callback to hide AutoPiP overlay view semi-opaque background layer.
  SetCloseCallback(std::move(hide_view_cb));

  set_use_custom_frame(true);
  set_margins(kBubbleMargins);
  set_title_margins(kBubbleTitleMargins);

  set_close_on_deactivate(false);

  anchor_view_observer_ =
      std::make_unique<AutoPipSettingView::AnchorViewObserver>(anchor_view,
                                                               this);

  // Init Bubble title view.
  InitBubbleTitleView(origin);

  // Initialize Bubble.
  InitBubble();
}

AutoPipSettingView::~AutoPipSettingView() {
  autopip_description_ = nullptr;
  origin_label_ = nullptr;
  allow_once_button_ = allow_on_every_visit_button_ = block_button_ = nullptr;
  anchor_view_observer_.reset();
  dialog_title_view_.reset();
}

void AutoPipSettingView::InitBubble() {
  std::unique_ptr<views::View> primary_view = std::make_unique<views::View>();

  auto* layout_manager =
      primary_view->SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout_manager->set_between_child_spacing(kLayoutBetweenChildSpacing);

  auto* description_view = primary_view->AddChildView(
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

  auto* controls_view = primary_view->AddChildView(
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

  SetContentsView(std::move(primary_view));
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
  button->SetID(static_cast<int>(ui_result));
  return button;
}

void AutoPipSettingView::InitBubbleTitleView(const GURL& origin) {
  DCHECK(origin.has_host() || origin.SchemeIsFile());

  dialog_title_view_ = std::make_unique<views::View>();
  auto* layout_manager = dialog_title_view_->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal);

  dialog_title_view_->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .Build());

  // For file URLs, we want to elide the tail, since the file name and/or query
  // part of the file URL can be made to look like an origin for spoofing. For
  // HTTPS URLs, we elide the head to prevent spoofing via long origins, since
  // in the HTTPS case everything besides the origin is removed for display.
  auto elide_behavior =
      origin.SchemeIsFile() ? gfx::ELIDE_TAIL : gfx::ELIDE_HEAD;
  // Determining the origin of a file URL is left as an exercise to the reader
  // https://url.spec.whatwg.org/#concept-url-origin. Therefore, for URLs with a
  // file scheme which do not have an origin, we use a default string.
  //
  // TODO(crbug.com/1485611): Investigate what to display as the origin for file
  // URLs hosted locally.
  const std::u16string host = (origin.SchemeIsFile() && !origin.has_host())
                                  ? u"localhost"
                                  : url_formatter::IDNToUnicode(origin.host());

  dialog_title_view_->AddChildView(views::Builder<views::Label>()
                                       .CopyAddressTo(&origin_label_)
                                       .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                                       .SetElideBehavior(elide_behavior)
                                       .SetMultiLine(false)
                                       .SetText(host)
                                       .Build());
  origin_label_->SetMaximumWidthSingleLine(kBubbleOriginLabelMaximumWidth);

  dialog_title_view_->AddChildView(views::Builder<views::Label>()
                                       .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                                       .SetElideBehavior(gfx::NO_ELIDE)
                                       .SetMultiLine(false)
                                       .SetText(kBubbleTitleSuffix)
                                       .Build());
}

void AutoPipSettingView::OnButtonPressed(UiResult result) {
  CHECK(result_cb_);

  std::move(result_cb_).Run(result);

  // Close the widget.
  GetWidget()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// views::BubbleDialogDelegate:
gfx::Rect AutoPipSettingView::GetAnchorRect() const {
  const auto anchor_rect = BubbleDialogDelegate::GetAnchorRect();
  const auto old_origin = anchor_rect.origin();
  const auto old_size = anchor_rect.size();
  const auto new_anchor_rect =
      gfx::Rect(old_origin.x(), old_origin.y() + kBubbleTopOffset,
                old_size.width(), old_size.height());
  return new_anchor_rect;
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate:
std::unique_ptr<views::NonClientFrameView>
AutoPipSettingView::CreateNonClientFrameView(views::Widget* widget) {
  // Create the customized bubble border.
  std::unique_ptr<views::BubbleBorder> bubble_border =
      std::make_unique<views::BubbleBorder>(
          arrow(), views::BubbleBorder::STANDARD_SHADOW);
  bubble_border->SetCornerRadius(kBubbleBorderCornerRadius);
  bubble_border->set_md_shadow_elevation(kBubbleBorderMdShadowElevation);
  bubble_border->set_draw_border_stroke(true);

  auto frame = BubbleDialogDelegate::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

void AutoPipSettingView::OnWidgetInitialized() {
  GetBubbleFrameView()->SetTitleView(std::move(dialog_title_view_));
}

///////////////////////////////////////////////////////////////////////////////
// AnchorViewObserver:
AutoPipSettingView::AnchorViewObserver::AnchorViewObserver(
    views::View* anchor_view,
    AutoPipSettingView* bubble)
    : bubble_(bubble) {
  observation_.Observe(anchor_view);
}
AutoPipSettingView::AnchorViewObserver::~AnchorViewObserver() = default;

void AutoPipSettingView::AnchorViewObserver::OnViewRemovedFromWidget(
    views::View*) {
  CloseWidget();
}
void AutoPipSettingView::AnchorViewObserver::OnViewIsDeleting(views::View*) {
  CloseWidget();
}

void AutoPipSettingView::AnchorViewObserver::CloseWidget() {
  observation_.Reset();
  if (bubble_->GetWidget() && !bubble_->GetWidget()->IsClosed()) {
    bubble_->GetWidget()->Close();
  }
}
