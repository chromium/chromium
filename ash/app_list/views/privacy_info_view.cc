// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/privacy_info_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "base/bind.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int kRowMarginDip = 4;
constexpr int kVerticalPaddingDip = 9;
constexpr int kLeftPaddingDip = 14;
constexpr int kRightPaddingDip = 4;
constexpr int kCellSpacingDip = 18;
constexpr int kIconSizeDip = 20;

// Text view used inside the privacy notice.
class PrivacyTextView : public views::StyledLabel {
 public:
  explicit PrivacyTextView(PrivacyInfoView* privacy_view)
      : StyledLabel(), privacy_view_(privacy_view) {}

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    switch (action_data.action) {
      case ax::mojom::Action::kDoDefault:
        privacy_view_->LinkClicked();
        return true;
      default:
        break;
    }
    return views::StyledLabel::HandleAccessibleAction(action_data);
  }

 private:
  PrivacyInfoView* const privacy_view_;  // Not owned.
};

}  // namespace

PrivacyInfoView::PrivacyInfoView(const int info_string_id,
                                 const int link_string_id)
    : SearchResultBaseView(),
      info_string_id_(info_string_id),
      link_string_id_(link_string_id) {
  InitLayout();
  // This view behaves like a container and should not hold focus, but instead
  // pass focus to its children.
  SetFocusBehavior(FocusBehavior::NEVER);
}

PrivacyInfoView::~PrivacyInfoView() = default;

gfx::Size PrivacyInfoView::CalculatePreferredSize() const {
  const int preferred_width = views::View::CalculatePreferredSize().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int PrivacyInfoView::GetHeightForWidth(int width) const {
  const int used_width = kRowMarginDip + kLeftPaddingDip + info_icon_->width() +
                         kCellSpacingDip +
                         /*|text_view_| is here*/
                         kCellSpacingDip + close_button_->width() +
                         kRightPaddingDip + kRowMarginDip;
  const int available_width = width - used_width;
  const int text_height = text_view_->GetHeightForWidth(available_width);
  return kRowMarginDip + /*border*/ 1 + kVerticalPaddingDip + text_height +
         kVerticalPaddingDip + /*border*/ 1 + kRowMarginDip;
}

void PrivacyInfoView::OnPaintBackground(gfx::Canvas* canvas) {
  if (selected_action_ == Action::kCloseButton) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(
        AppListColorProvider::Get()->GetSearchResultViewHighlightColor(),
        0x14));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(close_button_->bounds().CenterPoint(),
                       close_button_->width() / 2, flags);
  }
}

void PrivacyInfoView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // ChromeVox does not support nested buttons, so reassign this view's role to
  // be a container. This allows ChromeVox to focus onto the text view and close
  // button.
  node_data->role = ax::mojom::Role::kGenericContainer;
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

void PrivacyInfoView::OnKeyEvent(ui::KeyEvent* event) {
  if (event->key_code() == ui::VKEY_RETURN) {
    switch (selected_action_) {
      case Action::kTextLink:
        LinkClicked();
        break;
      case Action::kCloseButton:
        CloseButtonPressed();
        break;
      case Action::kNone:
        break;
    }
  }
}

void PrivacyInfoView::SelectInitialResultAction(bool reverse_tab_order) {
  if (!reverse_tab_order) {
    selected_action_ = Action::kTextLink;
    text_view_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  } else {
    selected_action_ = Action::kCloseButton;
    close_button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  }

  // Update visual indicators for focus.
  UpdateLinkStyle();
  SchedulePaint();
}

bool PrivacyInfoView::SelectNextResultAction(bool reverse_tab_order) {
  bool action_changed = false;

  // There are two traversal elements, the text view and close button.
  if (!reverse_tab_order && selected_action_ == Action::kTextLink) {
    // Move selection forward from the text view to the close button.
    selected_action_ = Action::kCloseButton;
    close_button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
    action_changed = true;
  } else if (reverse_tab_order && selected_action_ == Action::kCloseButton) {
    // Move selection backward from the close button to the text view.
    selected_action_ = Action::kTextLink;
    text_view_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
    action_changed = true;
  } else {
    selected_action_ = Action::kNone;
  }

  // Update visual indicators for focus.
  UpdateLinkStyle();
  SchedulePaint();
  return action_changed;
}

void PrivacyInfoView::NotifyA11yResultSelected() {
  switch (selected_action_) {
    case Action::kTextLink:
      text_view_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
      break;
    case Action::kCloseButton:
      close_button_->NotifyAccessibilityEvent(ax::mojom::Event::kSelection,
                                              true);
      break;
    case Action::kNone:
      break;
  }
}

void PrivacyInfoView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  if (sender == close_button_)
    CloseButtonPressed();
}

void PrivacyInfoView::InitLayout() {
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kVerticalPaddingDip, kLeftPaddingDip, kVerticalPaddingDip,
                  kRightPaddingDip),
      kCellSpacingDip));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBorder(views::CreateRoundedRectBorder(
      /*thickness=*/1,
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::EMPHASIS_MEDIUM),
      gfx::Insets(kRowMarginDip, kRowMarginDip), gfx::kGoogleGrey300));

  // Info icon.
  InitInfoIcon();

  // Text.
  InitText();

  // Set flex so that text takes up the right amount of horizontal space
  // between the info icon and close button.
  layout_manager->SetFlexForView(text_view_, 1);

  // Close button.
  InitCloseButton();
}

void PrivacyInfoView::InitInfoIcon() {
  info_icon_ = AddChildView(std::make_unique<views::ImageView>());
  info_icon_->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  info_icon_->SetImage(gfx::CreateVectorIcon(views::kInfoIcon, kIconSizeDip,
                                             gfx::kGoogleBlue600));
}

void PrivacyInfoView::InitText() {
  const base::string16 link = l10n_util::GetStringUTF16(link_string_id_);
  size_t offset;
  const base::string16 text =
      l10n_util::GetStringFUTF16(info_string_id_, link, &offset);
  text_view_ = AddChildView(std::make_unique<PrivacyTextView>(this));
  text_view_->SetText(text);
  text_view_->SetAutoColorReadabilityEnabled(false);
  text_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
  // Make the whole text view behave as a link for accessibility.
  text_view_->GetViewAccessibility().OverrideRole(ax::mojom::Role::kLink);

  views::StyledLabel::RangeStyleInfo style;
  style.override_color = AppListColorProvider::Get()->GetSearchBoxTextColor(
      kDeprecatedSearchBoxTextDefaultColor);
  text_view_->AddStyleRange(gfx::Range(0, offset), style);

  // Create a custom view for the link portion of the text. This allows an
  // underline font style to be applied when the link is focused. This is done
  // manually because default focus handling remains on the search box.
  views::StyledLabel::RangeStyleInfo link_style;
  link_style.disable_line_wrapping = true;
  auto custom_view = std::make_unique<views::Link>(link);
  custom_view->set_callback(base::BindRepeating(&PrivacyInfoView::LinkClicked,
                                                base::Unretained(this)));
  custom_view->SetEnabledColor(gfx::kGoogleBlue700);
  link_style.custom_view = custom_view.get();
  link_view_ = custom_view.get();
  text_view_->AddCustomView(std::move(custom_view));
  text_view_->AddStyleRange(gfx::Range(offset, offset + link.length()),
                            link_style);
}

void PrivacyInfoView::InitCloseButton() {
  auto close_button = std::make_unique<views::ImageButton>(this);
  close_button->SetImage(views::ImageButton::STATE_NORMAL,
                         gfx::CreateVectorIcon(views::kCloseIcon, kIconSizeDip,
                                               gfx::kGoogleGrey700));
  close_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  base::string16 close_button_label(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SetAccessibleName(close_button_label);
  close_button->SetTooltipText(close_button_label);
  close_button->SetFocusBehavior(FocusBehavior::ALWAYS);
  constexpr int kImageButtonSizeDip = 40;
  constexpr int kIconMarginDip = (kImageButtonSizeDip - kIconSizeDip) / 2;
  close_button->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kIconMarginDip)));
  close_button->SizeToPreferredSize();

  // Ink ripple.
  close_button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  constexpr SkColor kInkDropBaseColor = gfx::kGoogleGrey900;
  constexpr float kInkDropVisibleOpacity = 0.06f;
  constexpr float kInkDropHighlightOpacity = 0.08f;
  close_button->SetInkDropVisibleOpacity(kInkDropVisibleOpacity);
  close_button->SetInkDropHighlightOpacity(kInkDropHighlightOpacity);
  close_button->SetInkDropBaseColor(kInkDropBaseColor);
  close_button->SetHasInkDropActionOnClick(true);
  views::InstallCircleHighlightPathGenerator(close_button.get());
  close_button_ = AddChildView(std::move(close_button));
}

void PrivacyInfoView::UpdateLinkStyle() {
  bool link_selected = selected_action_ == Action::kTextLink;
  link_view_->SetForceUnderline(link_selected);
}

}  // namespace ash
