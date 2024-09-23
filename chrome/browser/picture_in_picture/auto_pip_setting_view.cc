// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"

#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/layout/flex_layout_view.h"

// Represents the bubble top border offset, with respect to the
// Picture-in-Picture window title bar. Used to allow the Bubble to overlap the
// title bar.
constexpr int kBubbleTopOffset = -4;

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

// Bubble fixed width.
constexpr int kBubbleFixedWidth = 320;

// Bubble border corner radius.
constexpr int kBubbleBorderCornerRadius = 12;

// Bubble border MD shadow elevation.
constexpr int kBubbleBorderMdShadowElevation = 2;

// Bubble margins.
constexpr gfx::Insets kBubbleMargins = gfx::Insets::TLBR(0, 20, 20, 20);

// Bubble title margins.
constexpr gfx::Insets kBubbleTitleMargins = gfx::Insets::TLBR(20, 20, 10, 20);

// Maximum origin text width, for cases where the origin needs to be
// elided.
constexpr int kBubbleOriginTextMaximumWidth = 230;

// Control view margins. The bubble control view refers to the view containing
// the permission buttons.
constexpr gfx::Insets kControlViewMargins = gfx::Insets::TLBR(8, 0, 0, 0);

AutoPipSettingView::AutoPipSettingView(
    ResultCb result_cb,
    HideViewCb hide_view_cb,
    const GURL& origin,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow)
    : views::BubbleDialogDelegate(anchor_view, arrow),
      result_cb_(std::move(result_cb)) {
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
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
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_BODY_3)
          .SetText(l10n_util::GetStringUTF16(
              IDS_AUTO_PICTURE_IN_PICTURE_DESCRIPTION))
          .Build());
  autopip_description_->SetSize(
      gfx::Size(kDescriptionViewWidth, kDescriptionViewHeight));

  auto* controls_view = primary_view->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(kLayoutBetweenChildSpacing)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart)
          .SetInsideBorderInsets(kControlViewMargins)
          .Build());

  allow_once_button_ = InitControlViewButton(
      controls_view, UiResult::kAllowOnce,
      l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_THIS_TIME));
  allow_on_every_visit_button_ = InitControlViewButton(
      controls_view, UiResult::kAllowOnEveryVisit,
      l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW_EVERY_VISIT));
  block_button_ = InitControlViewButton(
      controls_view, UiResult::kBlock,
      l10n_util::GetStringUTF16(IDS_PERMISSION_DONT_ALLOW));

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
  // file scheme which do not have an origin, we use the entire URL spec.
  const std::u16string host = (origin.SchemeIsFile() && !origin.has_host())
                                  ? base::UTF8ToUTF16(origin.spec())
                                  : url_formatter::IDNToUnicode(origin.host());
  origin_text_ = gfx::ElideText(host, gfx::FontList(),
                                kBubbleOriginTextMaximumWidth, elide_behavior);

  dialog_title_view_->AddChildView(
      views::Builder<views::Label>()
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetElideBehavior(gfx::NO_ELIDE)
          .SetMultiLine(false)
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetTextStyle(views::style::STYLE_HEADLINE_4)
          .SetText(l10n_util::GetStringFUTF16(IDS_PERMISSIONS_BUBBLE_PROMPT,
                                              origin_text_))
          .Build());
}

void AutoPipSettingView::OnButtonPressed(UiResult result) {
  CHECK(result_cb_);

  std::move(result_cb_).Run(result);

  // Close the widget.
  GetWidget()->Close();
}

bool AutoPipSettingView::WantsEvent(const gfx::Point& point_in_screen) {
  return allow_once_button_->HitTestPoint(views::View::ConvertPointFromScreen(
             allow_once_button_, point_in_screen)) ||
         allow_on_every_visit_button_->HitTestPoint(
             views::View::ConvertPointFromScreen(allow_on_every_visit_button_,
                                                 point_in_screen)) ||
         block_button_->HitTestPoint(views::View::ConvertPointFromScreen(
             block_button_, point_in_screen));
}

///////////////////////////////////////////////////////////////////////////////
// views::BubbleDialogDelegate:
gfx::Rect AutoPipSettingView::GetAnchorRect() const {
  const auto anchor_rect = BubbleDialogDelegate::GetAnchorRect();
  // If arrow is FLOAT, do not offset the anchor rect. This ensures that the
  // widget is centered for video pip windows.
  if (arrow() == views::BubbleBorder::Arrow::FLOAT) {
    return anchor_rect;
  }

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
