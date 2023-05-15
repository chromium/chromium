// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_container_view.h"

#include "ash/public/cpp/shelf_config.h"
#include "ui/compositor/layer.h"

namespace ash {

ShelfContainerView::ShelfContainerView(ShelfView* shelf_view)
    : shelf_view_(shelf_view) {}

ShelfContainerView::~ShelfContainerView() = default;

void ShelfContainerView::Initialize() {
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  shelf_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  shelf_view_->layer()->SetFillsBoundsOpaquely(false);
  AddChildView(shelf_view_.get());
}

gfx::Size ShelfContainerView::CalculateIdealSize(int button_size) const {
  const int button_strip_size = shelf_view_->GetSizeOfAppButtons(
      shelf_view_->number_of_visible_apps(), button_size);
  return shelf_view_->shelf()->IsHorizontalAlignment()
             ? gfx::Size(button_strip_size, button_size)
             : gfx::Size(button_size, button_strip_size);
}

gfx::Size ShelfContainerView::CalculatePreferredSize() const {
  return CalculateIdealSize(shelf_view_->GetButtonSize());
}

void ShelfContainerView::ChildPreferredSizeChanged(views::View* child) {
  // The CL (https://crrev.com/c/1876128) modifies View::PreferredSizeChanged
  // by moving InvalidateLayout() after ChildPreferredSizeChanged(). Meanwhile,
  // the parent view of ShelfContainerView overrides ChildPreferredSizeChanged
  // with calling Layout(). Due to the CL above, ShelfContainerView is not
  // labeled as |needs_layout_| when the parent view updates the layout. As a
  // result, Calling Layout() in the parent view may not trigger the update in
  // child view. So we have to invalidate the layout here explicitly.
  InvalidateLayout();

  PreferredSizeChanged();
}

const char* ShelfContainerView::GetClassName() const {
  return "ShelfContainerView";
}

void ShelfContainerView::TranslateShelfView(const gfx::Vector2dF& offset) {
  gfx::Transform transform_matrix;
  transform_matrix.Translate(-offset);
  shelf_view_->SetTransform(transform_matrix);
  shelf_view_->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                        true);
}

}  // namespace ash
