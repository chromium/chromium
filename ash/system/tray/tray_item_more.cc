// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_item_more.h"

#include <memory>

#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

TrayItemMore::TrayItemMore(SystemTrayItem* owner)
    : ActionableView(owner, TrayPopupInkDropStyle::FILL_BOUNDS),
      tri_view_(TrayPopupUtils::CreateDefaultRowView()),
      icon_(TrayPopupUtils::CreateMainImageView()),
      label_(TrayPopupUtils::CreateDefaultLabel()),
      more_(TrayPopupUtils::CreateMoreImageView()) {
  AddChildView(tri_view_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  tri_view_->AddView(TriView::Container::START, icon_);
  tri_view_->AddView(TriView::Container::CENTER, label_);
  tri_view_->AddView(TriView::Container::END, more_);

  SetInkDropMode(InkDropMode::ON);
}

TrayItemMore::~TrayItemMore() = default;

void TrayItemMore::SetLabel(const base::string16& label) {
  label_->SetText(label);
  Layout();
  SchedulePaint();
}

void TrayItemMore::SetImage(const gfx::ImageSkia& image_skia) {
  icon_->SetImage(image_skia);
  SchedulePaint();
}

std::unique_ptr<TrayPopupItemStyle> TrayItemMore::CreateStyle() const {
  std::unique_ptr<TrayPopupItemStyle> style = HandleCreateStyle();
  if (!enabled())
    style->set_color_style(TrayPopupItemStyle::ColorStyle::DISABLED);
  return style;
}

std::unique_ptr<TrayPopupItemStyle> TrayItemMore::HandleCreateStyle() const {
  return std::make_unique<TrayPopupItemStyle>(
      TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
}

void TrayItemMore::UpdateStyle() {
  std::unique_ptr<TrayPopupItemStyle> style = CreateStyle();
  style->SetupLabel(label_);
}

bool TrayItemMore::PerformAction(const ui::Event& event) {
  owner()->TransitionDetailedView();
  return true;
}

void TrayItemMore::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleName());
}

void TrayItemMore::OnEnabledChanged() {
  ActionableView::OnEnabledChanged();
  tri_view_->SetContainerVisible(TriView::Container::END, enabled());
  UpdateStyle();
}

void TrayItemMore::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  ActionableView::OnNativeThemeChanged(theme);
  UpdateStyle();
}

}  // namespace ash
