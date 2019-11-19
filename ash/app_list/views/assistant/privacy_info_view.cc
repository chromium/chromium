// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/privacy_info_view.h"

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/assistant/util/i18n_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int kRowMarginDip = 4;
constexpr int kLeftPaddingDip = 14;
constexpr int kRightPaddingDip = 4;
constexpr int kCellSpacingDip = 18;
constexpr int kIconSizeDip = 20;

}  // namespace

PrivacyInfoView::PrivacyInfoView(AppListViewDelegate* view_delegate,
                                 SearchResultPageView* search_result_page_view)
    : view_delegate_(view_delegate),
      search_result_page_view_(search_result_page_view) {
  InitLayout();
}

PrivacyInfoView::~PrivacyInfoView() = default;

gfx::Size PrivacyInfoView::CalculatePreferredSize() const {
  const int preferred_width = views::View::CalculatePreferredSize().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int PrivacyInfoView::GetHeightForWidth(int width) const {
  constexpr int kPreferredHeightDip = 48;
  return kPreferredHeightDip;
}

void PrivacyInfoView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  const int used_width = kRowMarginDip + kLeftPaddingDip + info_icon_->width() +
                         kCellSpacingDip +
                         /*|text_view_| is here*/
                         kCellSpacingDip + close_button_->width() +
                         kRightPaddingDip + kRowMarginDip;
  const int available_width = width() - used_width;
  text_view_->SizeToFit(available_width);
}

void PrivacyInfoView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      // Prevents closing the AppListView when a click event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void PrivacyInfoView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      // Prevents closing the AppListView when a tap event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void PrivacyInfoView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (sender != close_button_)
    return;

  view_delegate_->MarkAssistantPrivacyInfoDismissed();
  search_result_page_view_->OnAssistantPrivacyInfoViewCloseButtonPressed();
}

void PrivacyInfoView::StyledLabelLinkClicked(views::StyledLabel* label,
                                             const gfx::Range& range,
                                             int event_flags) {
  constexpr char url[] = "https://support.google.com/chromebook?p=assistant";
  view_delegate_->GetAssistantViewDelegate()->OpenUrlFromView(
      ash::assistant::util::CreateLocalizedGURL(url));
}

void PrivacyInfoView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kRowMarginDip)));
  row_container_ = new views::View();
  AddChildView(row_container_);

  constexpr int kVerticalPaddingDip = 0;
  auto* layout_manager =
      row_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(kVerticalPaddingDip, kLeftPaddingDip, kVerticalPaddingDip,
                      kRightPaddingDip),
          kCellSpacingDip));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  row_container_->SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/1,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::EMPHASIS_MEDIUM),
      gfx::kGoogleGrey300));

  // Info icon.
  InitInfoIcon();

  // Text.
  InitText();

  // Spacer.
  views::View* spacer = new views::View();
  row_container_->AddChildView(spacer);
  layout_manager->SetFlexForView(spacer, 1);

  // Close button.
  InitCloseButton();
}

void PrivacyInfoView::InitInfoIcon() {
  info_icon_ = new views::ImageView();
  info_icon_->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  info_icon_->SetImage(gfx::CreateVectorIcon(views::kInfoIcon, kIconSizeDip,
                                             gfx::kGoogleBlue600));
  row_container_->AddChildView(info_icon_);
}

void PrivacyInfoView::InitText() {
  const base::string16 link =
      l10n_util::GetStringUTF16(IDS_APP_LIST_LEARN_MORE);
  size_t offset;
  const base::string16 text = l10n_util::GetStringFUTF16(
      IDS_APP_LIST_ASSISTANT_PRIVACY_INFO, link, &offset);
  text_view_ = new views::StyledLabel(text, this);
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = text_view_->GetDefaultFontList().Derive(
      0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL);
  style.override_color = gfx::kGoogleGrey900;
  text_view_->AddStyleRange(gfx::Range(0, offset), style);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.override_color = gfx::kGoogleBlue700;
  text_view_->AddStyleRange(gfx::Range(offset, offset + link.length()),
                            link_style);
  text_view_->SetAutoColorReadabilityEnabled(false);
  row_container_->AddChildView(text_view_);
}

void PrivacyInfoView::InitCloseButton() {
  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::ImageButton::STATE_NORMAL,
                          gfx::CreateVectorIcon(views::kCloseIcon, kIconSizeDip,
                                                gfx::kGoogleGrey700));
  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  base::string16 close_button_label(
      l10n_util::GetStringUTF16(IDS_APP_LIST_ASSISTANT_PRIVACY_INFO_CLOSE));
  close_button_->SetAccessibleName(close_button_label);
  close_button_->SetTooltipText(close_button_label);
  close_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  constexpr int kImageButtonSizeDip = 40;
  constexpr int kIconMarginDip = (kImageButtonSizeDip - kIconSizeDip) / 2;
  close_button_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kIconMarginDip)));
  close_button_->SizeToPreferredSize();

  // Ink ripple.
  close_button_->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  constexpr SkColor kInkDropBaseColor = gfx::kGoogleGrey900;
  constexpr float kInkDropVisibleOpacity = 0.06f;
  constexpr float kInkDropHighlightOpacity = 0.08f;
  close_button_->set_ink_drop_visible_opacity(kInkDropVisibleOpacity);
  close_button_->set_ink_drop_highlight_opacity(kInkDropHighlightOpacity);
  close_button_->set_ink_drop_base_color(kInkDropBaseColor);
  close_button_->set_has_ink_drop_action_on_click(true);
  views::InstallCircleHighlightPathGenerator(close_button_);
  row_container_->AddChildView(close_button_);
}

}  // namespace ash
