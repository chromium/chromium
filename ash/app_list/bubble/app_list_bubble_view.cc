// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble_view.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

AppListBubbleView::AppListBubbleView()
    : views::BubbleDialogDelegateView(/*anchor_view=*/nullptr,
                                      views::BubbleBorder::BOTTOM_LEFT) {
  // TODO(https://crbug.com/1204554): Support BubbleBorder::TOP_LEFT and
  // TOP_RIGHT for side-aligned shelf.

  SetButtons(ui::DIALOG_BUTTON_NONE);

  // TODO(https://crbug.com/1204554): Multi-display support.
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  set_parent_window(
      Shell::GetContainer(root_window, kShellWindowId_AppListContainer));

  // TODO(https://crbug.com/1204554): Anchor to launcher button rect.
  SetAnchorRect(Shelf::ForWindow(root_window)->GetShelfBoundsInScreen());

  auto* layout = SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  // TODO(https://crbug.com/1204551): Create real contents.
  AddChildView(std::make_unique<views::Label>(u"Placeholder"));
}

AppListBubbleView::~AppListBubbleView() = default;

}  // namespace ash
