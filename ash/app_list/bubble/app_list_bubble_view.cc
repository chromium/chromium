// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble_view.h"

#include <memory>

#include "ash/app_list/bubble/bubble_apps_page.h"
#include "ash/app_list/bubble/bubble_assistant_page.h"
#include "ash/app_list/bubble/bubble_search_page.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "base/i18n/rtl.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;
using views::BubbleBorder;

namespace ash {
namespace {

// Returns the point on the screen to which the bubble is anchored.
gfx::Point GetAnchorPointInScreen(aura::Window* root_window,
                                  ShelfAlignment shelf_alignment) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  switch (shelf_alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return base::i18n::IsRTL() ? display.work_area().bottom_right()
                                 : display.work_area().bottom_left();
    case ShelfAlignment::kLeft:
      return display.work_area().origin();
    case ShelfAlignment::kRight:
      return display.work_area().top_right();
  }
}

// Returns which corner of the bubble is anchored. The views bubble code calls
// this "arrow" for historical reasons. No arrow is drawn.
BubbleBorder::Arrow GetArrowCorner(ShelfAlignment shelf_alignment) {
  switch (shelf_alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return base::i18n::IsRTL() ? BubbleBorder::BOTTOM_RIGHT
                                 : BubbleBorder::BOTTOM_LEFT;
    case ShelfAlignment::kLeft:
      return BubbleBorder::TOP_LEFT;
    case ShelfAlignment::kRight:
      return BubbleBorder::TOP_RIGHT;
  }
}

}  // namespace

AppListBubbleView::AppListBubbleView(aura::Window* root_window,
                                     ShelfAlignment shelf_alignment) {
  DCHECK(root_window);
  // The bubble is anchored to a screen corner point, but the API takes a rect.
  SetAnchorRect(gfx::Rect(GetAnchorPointInScreen(root_window, shelf_alignment),
                          gfx::Size()));
  SetArrow(GetArrowCorner(shelf_alignment));

  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_parent_window(
      Shell::GetContainer(root_window, kShellWindowId_AppListContainer));

  auto* layout = SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  // TODO(https://crbug.com/1204551): Replace with real search box.
  auto* textfield = AddChildView(std::make_unique<views::Textfield>());
  SetInitiallyFocusedView(textfield);

  // TODO(https://crbug.com/1204551): Remove when search box is hooked up.
  AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&AppListBubbleView::FlipPage, base::Unretained(this)),
      u"Flip page"));

  apps_page_ = AddChildView(std::make_unique<BubbleAppsPage>());

  search_page_ = AddChildView(std::make_unique<BubbleSearchPage>());
  search_page_->SetVisible(false);

  assistant_page_ = AddChildView(std::make_unique<BubbleAssistantPage>());
  assistant_page_->SetVisible(false);
}

AppListBubbleView::~AppListBubbleView() = default;

gfx::Size AppListBubbleView::CalculatePreferredSize() const {
  constexpr gfx::Size kDefaultSizeDips(600, 550);
  // TODO(https://crbug.com/1210522): Adjust size based on screen resolution.
  return kDefaultSizeDips;
}

void AppListBubbleView::FlipPage() {
  ++visible_page_;
  visible_page_ %= 3;
  apps_page_->SetVisible(visible_page_ == 0);
  search_page_->SetVisible(visible_page_ == 1);
  assistant_page_->SetVisible(visible_page_ == 2);
}

}  // namespace ash
