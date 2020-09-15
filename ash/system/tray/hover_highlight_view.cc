// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/hover_highlight_view.h"

#include <string>
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/tray/unfocusable_label.h"
#include "ash/system/tray/view_click_listener.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

HoverHighlightView::HoverHighlightView(ViewClickListener* listener)
    : HoverHighlightView(listener, true) {}

HoverHighlightView::HoverHighlightView(ViewClickListener* listener,
                                       bool use_unified_theme)
    : ActionableView(TrayPopupInkDropStyle::FILL_BOUNDS),
      listener_(listener),
      use_unified_theme_(use_unified_theme) {
  SetNotifyEnterExitOnChild(true);
  SetInkDropMode(InkDropMode::ON);
}

HoverHighlightView::~HoverHighlightView() = default;

void HoverHighlightView::AddRightIcon(const gfx::ImageSkia& image,
                                      int icon_size) {
  DCHECK(is_populated_);
  DCHECK(!right_view_);

  views::ImageView* right_icon = TrayPopupUtils::CreateMainImageView();
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

  if (border)
    tri_view_->SetContainerBorder(TriView::Container::END, std::move(border));

  right_view_ = view;
  right_view_->SetEnabled(GetEnabled());
  tri_view_->AddView(TriView::Container::END, right_view_);
  tri_view_->SetContainerVisible(TriView::Container::END, true);
}

void HoverHighlightView::SetRightViewVisible(bool visible) {
  DCHECK(is_populated_);
  if (!right_view_)
    return;

  tri_view_->SetContainerVisible(TriView::Container::END, visible);
  right_view_->SetVisible(visible);
  Layout();
}

void HoverHighlightView::SetSubText(const base::string16& sub_text) {
  DCHECK(is_populated_);
  DCHECK(text_label_);
  DCHECK(!sub_text.empty());

  if (!sub_text_label_) {
    sub_text_label_ = TrayPopupUtils::CreateUnfocusableLabel();
    tri_view_->AddView(TriView::Container::CENTER, sub_text_label_);
  }

  TrayPopupItemStyle sub_style(TrayPopupItemStyle::FontStyle::CAPTION,
                               use_unified_theme_);
  sub_style.set_color_style(TrayPopupItemStyle::ColorStyle::INACTIVE);
  sub_style.SetupLabel(sub_text_label_);
  sub_text_label_->SetText(sub_text);
}

void HoverHighlightView::AddIconAndLabel(const gfx::ImageSkia& image,
                                         const base::string16& text) {
  DoAddIconAndLabel(image, text,
                    TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
}

void HoverHighlightView::DoAddIconAndLabel(
    const gfx::ImageSkia& image,
    const base::string16& text,
    TrayPopupItemStyle::FontStyle font_style) {
  DCHECK(!is_populated_);
  is_populated_ = true;

  SetLayoutManager(std::make_unique<views::FillLayout>());
  tri_view_ = TrayPopupUtils::CreateDefaultRowView();
  AddChildView(tri_view_);

  left_icon_ = TrayPopupUtils::CreateMainImageView();
  left_icon_->SetImage(image);
  left_icon_->SetEnabled(GetEnabled());
  tri_view_->AddView(TriView::Container::START, left_icon_);

  text_label_ = TrayPopupUtils::CreateUnfocusableLabel();
  text_label_->SetText(text);
  text_label_->SetEnabled(GetEnabled());
  TrayPopupItemStyle style(font_style, use_unified_theme_);
  style.SetupLabel(text_label_);
  tri_view_->AddView(TriView::Container::CENTER, text_label_);
  // By default, END container is invisible, so labels in the CENTER should have
  // an extra padding at the end.
  tri_view_->SetContainerBorder(
      TriView::Container::CENTER,
      views::CreateEmptyBorder(0, 0, 0, kTrayPopupLabelRightPadding));
  tri_view_->SetContainerVisible(TriView::Container::END, false);

  SetAccessibleName(text);
}

void HoverHighlightView::AddLabelRow(const base::string16& text) {
  DCHECK(!is_populated_);
  is_populated_ = true;

  SetLayoutManager(std::make_unique<views::FillLayout>());
  tri_view_ = TrayPopupUtils::CreateDefaultRowView();
  AddChildView(tri_view_);

  text_label_ = TrayPopupUtils::CreateUnfocusableLabel();
  text_label_->SetText(text);

  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL,
                           use_unified_theme_);
  style.SetupLabel(text_label_);
  tri_view_->AddView(TriView::Container::CENTER, text_label_);

  SetAccessibleName(text);
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
  if (accessibility_state_ != AccessibilityState::DEFAULT)
    NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
}

void HoverHighlightView::Reset() {
  RemoveAllChildViews(true);
  text_label_ = nullptr;
  sub_text_label_ = nullptr;
  left_icon_ = nullptr;
  right_view_ = nullptr;
  tri_view_ = nullptr;
  is_populated_ = false;
}

void HoverHighlightView::OnSetTooltipText(const base::string16& tooltip_text) {
  if (text_label_)
    text_label_->SetTooltipText(tooltip_text);
  if (sub_text_label_)
    sub_text_label_->SetTooltipText(tooltip_text);
  if (left_icon_)
    left_icon_->SetTooltipText(tooltip_text);
}

bool HoverHighlightView::PerformAction(const ui::Event& event) {
  if (!listener_)
    return false;
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

  if (accessibility_state_ == AccessibilityState::CHECKED_CHECKBOX)
    checked_state = ax::mojom::CheckedState::kTrue;
  else if (accessibility_state_ == AccessibilityState::UNCHECKED_CHECKBOX)
    checked_state = ax::mojom::CheckedState::kFalse;
  else
    return;  // Not a checkbox

  // Checkbox
  node_data->role = ax::mojom::Role::kCheckBox;
  node_data->SetCheckedState(checked_state);
}

const char* HoverHighlightView::GetClassName() const {
  return "HoverHighlightView";
}

gfx::Size HoverHighlightView::CalculatePreferredSize() const {
  gfx::Size size = ActionableView::CalculatePreferredSize();

  if (!expandable_ || size.height() < kTrayPopupItemMinHeight)
    size.set_height(kTrayPopupItemMinHeight);

  return size;
}

int HoverHighlightView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void HoverHighlightView::OnFocus() {
  ScrollRectToVisible(gfx::Rect(gfx::Point(), size()));
  ActionableView::OnFocus();
}

void HoverHighlightView::OnEnabledChanged() {
  if (left_icon_)
    left_icon_->SetEnabled(GetEnabled());
  if (text_label_)
    text_label_->SetEnabled(GetEnabled());
  if (right_view_)
    right_view_->SetEnabled(GetEnabled());
}

}  // namespace ash
