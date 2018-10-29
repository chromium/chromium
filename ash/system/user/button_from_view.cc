// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/system/user/button_from_view.h"

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace tray {

ButtonFromView::ButtonFromView(views::View* content,
                               views::ButtonListener* listener,
                               TrayPopupInkDropStyle ink_drop_style)
    : Button(listener),
      content_(content),
      ink_drop_style_(ink_drop_style),
      button_hovered_(false),
      ink_drop_container_(nullptr) {
  set_has_ink_drop_action_on_click(true);
  set_notify_enter_exit_on_child(true);
  ink_drop_container_ = new views::InkDropContainerView();
  AddChildView(ink_drop_container_);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetInkDropMode(InkDropMode::ON);
  AddChildView(content_);
  // Only make it focusable when we are active/interested in clicks.
  if (listener)
    SetFocusForPlatform();

  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());

  SetBackground(features::IsSystemTrayUnifiedEnabled()
                    ? views::CreateSolidBackground(kUnifiedMenuBackgroundColor)
                    : views::CreateThemedSolidBackground(
                          this, ui::NativeTheme::kColorId_BubbleBackground));
}

ButtonFromView::~ButtonFromView() = default;

void ButtonFromView::OnMouseEntered(const ui::MouseEvent& event) {
  button_hovered_ = true;
}

void ButtonFromView::OnMouseExited(const ui::MouseEvent& event) {
  button_hovered_ = false;
}

void ButtonFromView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);
  // If no label has been explicitly set via Button::SetAccessibleName(),
  // use the content's label.
  if (node_data->GetStringAttribute(ax::mojom::StringAttribute::kName)
          .empty()) {
    ui::AXNodeData content_data;
    content_->GetAccessibleNodeData(&content_data);
    node_data->SetName(
        content_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }
}

void ButtonFromView::Layout() {
  Button::Layout();
  if (ink_drop_container_)
    ink_drop_container_->SetBoundsRect(GetLocalBounds());
}

void ButtonFromView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  // TODO(bruthig): Rework InkDropHostView so that it can still manage the
  // creation/application of the mask while allowing subclasses to use an
  // InkDropContainer.
  ink_drop_mask_ = CreateInkDropMask();
  if (ink_drop_mask_)
    ink_drop_layer->SetMaskLayer(ink_drop_mask_->layer());
  ink_drop_container_->AddInkDropLayer(ink_drop_layer);
}

void ButtonFromView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  // TODO(bruthig): Rework InkDropHostView so that it can still manage the
  // creation/application of the mask while allowing subclasses to use an
  // InkDropContainer.
  // Layers safely handle destroying a mask layer before the masked layer.
  ink_drop_mask_.reset();
  ink_drop_container_->RemoveInkDropLayer(ink_drop_layer);
}

std::unique_ptr<views::InkDrop> ButtonFromView::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(this);
}

std::unique_ptr<views::InkDropRipple> ButtonFromView::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      ink_drop_style_, this, GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
ButtonFromView::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(ink_drop_style_, this);
}

std::unique_ptr<views::InkDropMask> ButtonFromView::CreateInkDropMask() const {
  return TrayPopupUtils::CreateInkDropMask(ink_drop_style_, this);
}

}  // namespace tray
}  // namespace ash
