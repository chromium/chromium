// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_submenu_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/quick_insert/views/quick_insert_list_item_view.h"
#include "ash/quick_insert/views/quick_insert_submenu_view.h"
#include "base/scoped_observation.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace ash {
namespace {

views::Widget::InitParams CreateInitParams(
    views::View* anchor_view,
    std::unique_ptr<views::WidgetDelegate> delegate) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_BUBBLE);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.autosize = true;
  // TODO(b/309706053): Replace this with the finalized string.
  params.name = "QuickInsertSubmenu";
  params.parent = anchor_view->GetWidget()->GetNativeWindow();
  params.delegate = delegate.release();
  return params;
}

}  // namespace

QuickInsertSubmenuController::QuickInsertSubmenuController() = default;

QuickInsertSubmenuController::~QuickInsertSubmenuController() = default;

void QuickInsertSubmenuController::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  if (!observed_view->IsDrawn()) {
    Close();
  }
}

void QuickInsertSubmenuController::OnViewIsDeleting(
    views::View* observed_view) {
  Close();
}

void QuickInsertSubmenuController::Show(
    views::View* anchor_view,
    std::vector<std::unique_ptr<QuickInsertListItemView>> items) {
  widget_ = std::make_unique<views::Widget>(CreateInitParams(
      anchor_view, std::make_unique<QuickInsertSubmenuView>(
                       anchor_view->GetBoundsInScreen(), std::move(items))));
  views::Widget::ReparentNativeView(widget_->GetNativeWindow(),
                                    anchor_view->GetWidget()->GetNativeView());
  widget_->Show();
  anchor_view_observation_.Reset();
  anchor_view_observation_.Observe(anchor_view);
  anchor_view_ = anchor_view;

  // This forces the Widget to reposition itself based on the anchor.
  widget_->OnRootViewLayoutInvalidated();
}

void QuickInsertSubmenuController::Close() {
  if (widget_) {
    widget_->Close();
  }
  anchor_view_ = nullptr;
  anchor_view_observation_.Reset();
}

QuickInsertSubmenuView* QuickInsertSubmenuController::GetSubmenuView() {
  if (!widget_) {
    return nullptr;
  }
  return static_cast<QuickInsertSubmenuView*>(widget_->widget_delegate());
}

views::View* QuickInsertSubmenuController::GetAnchorView() {
  return anchor_view_;
}

}  // namespace ash
