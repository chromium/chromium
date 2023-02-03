// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/hover_highlight_view.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/tray/unfocusable_label.h"
#include "ash/system/tray/view_click_listener.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"

namespace ash {

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS), listener_(listener) {
  SetNotifyEnterExitOnChild(true);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
}

HoverHighlightView::~HoverHighlightView() = default;

void HoverHighlightView::AddRightIcon(const ui::ImageModel& image,
                                      int icon_size) {
  DCHECK(is_populated_);
  DCHECK(!right_view_);

  views::ImageView* right_icon = TrayPopupUtils::CreateMainImageView(
      /*use_wide_layout=*/features::IsQsRevampEnabled());
  right_icon->SetImage(image);
  AddRightView(right_icon);
}

void HoverHighlightView::AddRightView(views::View* view,
                                      std::unique_ptr<views::Border> border) {
  DCHECK(is_populated_);
  DCHECK(!right_view_);

  // When a right view is added, extra padding on the CENTER container should be
  // removed.
  tri_view_->SetContainerBorder(TriView::Container::CENTER, nullptr);

  if (border) {
    tri_view_->SetContainerBorder(TriView::Container::END, std::move(border));
  }

  right_view_ = view;
  right_view_->SetEnabled(GetEnabled());
  tri_view_->AddView(TriView::Container::END, right_view_);
  tri_view_->SetContainerVisible(TriView::Container::END, true);
}

void HoverHighlightView::AddAdditionalRightView(views::View* view) {
  DCHECK(is_populated_);
  tri_view_->AddViewAt(TriView::Container::END, view, /*index=*/0);
}

void HoverHighlightView::SetRightViewVisible(bool visible) {
  DCHECK(is_populated_);
  if (!right_view_) {
    return;
  }

  tri_view_->SetContainerVisible(TriView::Container::END, visible);
  right_view_->SetVisible(visible);
  Layout();
}

void HoverHighlightView::SetSubText(const std::u16string& sub_text) {
  DCHECK(sub_row_);
  DCHECK(!sub_text.empty());

  if (!sub_text_label_) {
    sub_text_label_ =
        sub_row_->AddChildView(TrayPopupUtils::CreateUnfocusableLabel());
  }

  sub_text_label_->SetEnabledColorId(kColorAshTextColorSecondary);
  sub_text_label_->SetAutoColorReadabilityEnabled(false);
  sub_text_label_->SetText(sub_text);
}

void HoverHighlightView::AddIconAndLabel(const gfx::ImageSkia& image,
                                         const std::u16string& text) {
  DCHECK(!is_populated_);

  std::unique_ptr<views::ImageView> icon(TrayPopupUtils::CreateMainImageView(
      /*use_wide_layout=*/features::IsQsRevampEnabled()));
  icon->SetImage(image);
  icon->SetEnabled(GetEnabled());

  AddViewAndLabel(std::move(icon), text);
}

void HoverHighlightView::AddIconAndLabel(const ui::ImageModel& image,
                                         const std::u16string& text) {
  DCHECK(!is_populated_);

  std::unique_ptr<views::ImageView> icon(TrayPopupUtils::CreateMainImageView(
      /*use_wide_layout=*/features::IsQsRevampEnabled()));
  icon->SetImage(image);
  icon->SetEnabled(GetEnabled());

  AddViewAndLabel(std::move(icon), text);
}

void HoverHighlightView::AddViewAndLabel(std::unique_ptr<views::View> view,
                                         const std::u16string& text) {
  DCHECK(!is_populated_);
  DCHECK(view);
  is_populated_ = true;

  SetLayoutManager(std::make_unique<views::FillLayout>());
  tri_view_ = TrayPopupUtils::CreateDefaultRowView(
      /*use_wide_layout=*/features::IsQsRevampEnabled());
  AddChildView(tri_view_);

  left_view_ = view.get();
  tri_view_->AddView(TriView::Container::START, view.release());

  text_label_ = TrayPopupUtils::CreateUnfocusableLabel();
  text_label_->SetText(text);
  text_label_->SetEnabled(GetEnabled());
  text_label_->SetEnabledColorId(kColorAshTextColorPrimary);
  TrayPopupUtils::SetLabelFontList(
      text_label_, TrayPopupUtils::FontStyle::kDetailedViewLabel);
  tri_view_->AddView(TriView::Container::CENTER, text_label_);
  // By default, END container is invisible, so labels in the CENTER should have
  // an extra padding at the end.
  tri_view_->SetContainerBorder(TriView::Container::CENTER,
                                views::CreateEmptyBorder(gfx::Insets::TLBR(
                                    0, 0, 0, kTrayPopupLabelRightPadding)));
  tri_view_->SetContainerVisible(TriView::Container::END, false);

  AddSubRowContainer();

  SetAccessibleName(text);
}

void HoverHighlightView::AddLabelRow(const std::u16string& text) {
  DCHECK(!is_populated_);
  is_populated_ = true;

  SetLayoutManager(std::make_unique<views::FillLayout>());
  tri_view_ = TrayPopupUtils::CreateDefaultRowView(
      /*use_wide_layout=*/features::IsQsRevampEnabled());
  AddChildView(tri_view_);

  text_label_ = TrayPopupUtils::CreateUnfocusableLabel();
  text_label_->SetText(text);
  text_label_->SetEnabledColorId(kColorAshTextColorPrimary);
  TrayPopupUtils::SetLabelFontList(
      text_label_, TrayPopupUtils::FontStyle::kDetailedViewLabel);
  tri_view_->AddView(TriView::Container::CENTER, text_label_);

  AddSubRowContainer();

  SetAccessibleName(text);
}

void HoverHighlightView::AddLabelRow(const std::u16string& text,
                                     int start_inset) {
  AddLabelRow(text);

  tri_view_->SetMinSize(TriView::Container::START,
                        gfx::Size(start_inset, kTrayPopupItemMinHeight));
}

void HoverHighlightView::SetExpandable(bool expandable) {
  if (expandable != expandable_) {
    expandable_ = expandable;
    InvalidateLayout();
  }
}

void HoverHighlightView::SetAccessibilityState(
    AccessibilityState accessibility_state) {
  accessibility_state_ = accessibility_state;
  if (accessibility_state_ != AccessibilityState::DEFAULT) {
    NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
  }
}

void HoverHighlightView::Reset() {
  RemoveAllChildViews();
  text_label_ = nullptr;
  sub_text_label_ = nullptr;
  left_view_ = nullptr;
  right_view_ = nullptr;
  sub_row_ = nullptr;
  tri_view_ = nullptr;
  is_populated_ = false;
}

void HoverHighlightView::OnSetTooltipText(const std::u16string& tooltip_text) {
  if (text_label_) {
    text_label_->SetTooltipText(tooltip_text);
  }
  if (sub_text_label_) {
    sub_text_label_->SetTooltipText(tooltip_text);
  }
  if (left_view_) {
    DCHECK(views::IsViewClass<views::ImageView>(left_view_));
    static_cast<views::ImageView*>(left_view_)->SetTooltipText(tooltip_text);
  }
}

bool HoverHighlightView::PerformAction(const ui::Event& event) {
  if (!listener_) {
    return false;
  }
  listener_->OnViewClicked(this);
  return true;
}

void HoverHighlightView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (right_view_ && right_view_->GetVisible() &&
      std::string(right_view_->GetClassName()).find("Button") !=
          std::string::npos) {
    // Allow selection of sub-components.
    node_data->role = ax::mojom::Role::kGenericContainer;

    // Include "press search plus space to activate" when announcing.
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);

    node_data->SetName(GetAccessibleName());
    node_data->SetDescription(
        l10n_util::GetStringUTF16(IDS_ASH_A11Y_ROLE_BUTTON));
  } else {
    ActionableView::GetAccessibleNodeData(node_data);
  }

  ax::mojom::CheckedState checked_state;

  if (accessibility_state_ == AccessibilityState::CHECKED_CHECKBOX) {
    checked_state = ax::mojom::CheckedState::kTrue;
  } else if (accessibility_state_ == AccessibilityState::UNCHECKED_CHECKBOX) {
    checked_state = ax::mojom::CheckedState::kFalse;
  } else {
    return;  // Not a checkbox
  }

  // Checkbox
  node_data->role = ax::mojom::Role::kCheckBox;
  node_data->SetCheckedState(checked_state);
}

const char* HoverHighlightView::GetClassName() const {
  return "HoverHighlightView";
}

gfx::Size HoverHighlightView::CalculatePreferredSize() const {
  gfx::Size size = ActionableView::CalculatePreferredSize();

  if (!expandable_ || size.height() < kTrayPopupItemMinHeight) {
    size.set_height(kTrayPopupItemMinHeight);
  }

  return size;
}

int HoverHighlightView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void HoverHighlightView::OnFocus() {
  ScrollRectToVisible(gfx::Rect(gfx::Point(), size()));
  ActionableView::OnFocus();
}

void HoverHighlightView::AddSubRowContainer() {
  DCHECK(is_populated_);
  DCHECK(tri_view_);
  DCHECK(text_label_);
  DCHECK(!sub_row_);
  sub_row_ = new views::View();
  sub_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  tri_view_->AddView(TriView::Container::CENTER, sub_row_);
}

void HoverHighlightView::OnEnabledChanged() {
  if (left_view_) {
    left_view_->SetEnabled(GetEnabled());
  }
  if (text_label_) {
    text_label_->SetEnabled(GetEnabled());
  }
  if (right_view_) {
    right_view_->SetEnabled(GetEnabled());
  }
}

}  // namespace ash
