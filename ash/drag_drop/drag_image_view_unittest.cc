// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_image_view.h"

#include "ash/test/ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

using DragDropImageTest = AshTestBase;

TEST_F(DragDropImageTest, SetBoundsConsidersDragHintForTouch) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  views::UniqueWidgetPtr drag_image_widget =
      DragImageView::Create(window.get(), ui::mojom::DragEventSource::kTouch);
  DragImageView* drag_image_view =
      static_cast<DragImageView*>(drag_image_widget->GetContentsView());

  gfx::Size minimum_size = drag_image_view->GetMinimumSize();
  gfx::Point new_pos(5, 5);

  if (!minimum_size.IsEmpty()) {
    // Expect that the view is at least the size of the drag hint image.
    gfx::Rect small_bounds(0, 0, 1, 1);
    drag_image_view->SetBoundsInScreen(small_bounds);
    EXPECT_EQ(gfx::Rect(minimum_size), drag_image_view->GetBoundsInScreen());

    // Expect that we can change the position without affecting the bounds.
    drag_image_view->SetScreenPosition(new_pos);
    EXPECT_EQ(gfx::Rect(new_pos, minimum_size),
              drag_image_view->GetBoundsInScreen());
  }

  // Expect that we can resize the view normally.
  gfx::Rect large_bounds(0, 0, minimum_size.width() + 1,
                         minimum_size.height() + 1);
  drag_image_view->SetBoundsInScreen(large_bounds);
  EXPECT_EQ(large_bounds, drag_image_view->GetBoundsInScreen());
  // Expect that we can change the position without affecting the bounds.
  drag_image_view->SetScreenPosition(new_pos);
  EXPECT_EQ(gfx::Rect(new_pos, large_bounds.size()),
            drag_image_view->GetBoundsInScreen());
}

TEST_F(DragDropImageTest, SetBoundsIgnoresDragHintForMouse) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  views::UniqueWidgetPtr drag_image_widget =
      DragImageView::Create(window.get(), ui::mojom::DragEventSource::kMouse);
  DragImageView* drag_image_view =
      static_cast<DragImageView*>(drag_image_widget->GetContentsView());

  // Expect no drag hint image.
  gfx::Rect small_bounds(0, 0, 1, 1);
  drag_image_view->SetBoundsInScreen(small_bounds);
  EXPECT_EQ(small_bounds, drag_image_view->GetBoundsInScreen());
  EXPECT_EQ(drag_image_view->GetMinimumSize(),
            drag_image_view->GetBoundsInScreen().size());

  gfx::Point new_pos(5, 5);
  // Expect that we can change the position without affecting the bounds.
  drag_image_view->SetScreenPosition(new_pos);
  EXPECT_EQ(gfx::Rect(new_pos, small_bounds.size()),
            drag_image_view->GetBoundsInScreen());

  // Expect that we can resize the view.
  gfx::Rect large_bounds(0, 0, 100, 100);
  drag_image_view->SetBoundsInScreen(large_bounds);
  EXPECT_EQ(large_bounds, drag_image_view->GetBoundsInScreen());
  // Expect that we can change the position without affecting the bounds.
  drag_image_view->SetScreenPosition(new_pos);
  EXPECT_EQ(gfx::Rect(new_pos, large_bounds.size()),
            drag_image_view->GetBoundsInScreen());
}

}  // namespace ash
