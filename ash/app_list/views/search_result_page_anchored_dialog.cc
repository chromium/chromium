// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_page_anchored_dialog.h"

#include <utility>

#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The intended offset the dialog should have from the top of the anchor view
// bounds.
constexpr int kDialogVerticalMargin = 32;

}  // namespace

SearchResultPageAnchoredDialog::SearchResultPageAnchoredDialog(
    std::unique_ptr<views::DialogDelegateView> dialog,
    views::View* host_view,
    base::OnceClosure callback)
    : host_view_(host_view), callback_(std::move(callback)) {
  DCHECK(!dialog->GetWidget());

  views::Widget* const parent = host_view_->GetWidget();
  // The |dialog| ownership is passed to the window hierarchy.
  widget_ = views::DialogDelegate::CreateDialogWidget(
      dialog.release(), nullptr, parent->GetNativeWindow());
  widget_observations_.AddObservation(widget_);
  widget_observations_.AddObservation(parent);
}

SearchResultPageAnchoredDialog::~SearchResultPageAnchoredDialog() {
  widget_observations_.RemoveAllObservations();
  if (widget_)
    widget_->Close();
}

void SearchResultPageAnchoredDialog::UpdateBounds(
    const gfx::Rect& anchor_bounds) {
  if (!widget_)
    return;

  anchor_bounds_ = anchor_bounds;

  gfx::Point anchor_point_in_screen = anchor_bounds.CenterPoint();
  views::View::ConvertPointToScreen(host_view_, &anchor_point_in_screen);

  // Calculate dialog offset from the anchor view center so the dialog frame
  // (ignoring borders) respects kDialogVerticalMargin.
  const int vertical_offset =
      kDialogVerticalMargin - anchor_bounds.height() / 2 -
      widget_->non_client_view()->frame_view()->GetInsets().top();
  gfx::Size dialog_size = widget_->GetContentsView()->GetPreferredSize();
  widget_->SetBounds(
      gfx::Rect(gfx::Point(anchor_point_in_screen.x() - dialog_size.width() / 2,
                           anchor_point_in_screen.y() + vertical_offset),
                dialog_size));
}

float SearchResultPageAnchoredDialog::AdjustVerticalTransformOffset(
    float default_offset) {
  // In addition to the search box offset (in host view coordinates), the
  // widget has to consider the parent (app list view) widget transform to
  // correctly follow the anchor view animation.
  const float parent_offset =
      host_view_->GetWidget()->GetLayer()->transform().To2dTranslation().y();
  return default_offset + parent_offset;
}

void SearchResultPageAnchoredDialog::OnWidgetClosing(views::Widget* widget) {
  widget_ = nullptr;
  widget_observations_.RemoveAllObservations();
  if (callback_)
    std::move(callback_).Run();
}

void SearchResultPageAnchoredDialog::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  // Reposition the dialog widget if the host widget bounds change (if the
  // bounds size remained the same, the host widget bounds change may not cause
  // app list layout, and thus the anchor bounds in the host view coordinates
  // may not change).
  if (widget == host_view_->GetWidget())
    UpdateBounds(anchor_bounds_);
}

}  // namespace ash
