// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/assistant/app_list_bubble_assistant_page.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/i18n/rtl.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;
using views::BubbleBorder;

namespace ash {
namespace {

constexpr int kDefaultHeight = 688;

// As of August 2021 the assistant cards require a minimum width of 640. If the
// cards become narrower then this could be reduced.
constexpr int kDefaultWidth = 640;

// Space between the AppListBubbleView and the top of the screen should be at
// least this value plus the shelf height.
constexpr int kExtraTopOfScreenSpacing = 16;

gfx::Rect GetWorkAreaForBubble(aura::Window* root_window) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  gfx::Rect work_area = display.work_area();

  // Subtract the shelf's bounds from the work area, since the shelf should
  // always be shown with the app list bubble. This is done because the work
  // area includes the area under the shelf when the shelf is set to auto-hide.
  work_area.Subtract(Shelf::ForWindow(root_window)->GetIdealBounds());

  return work_area;
}

// Returns the point on the screen to which the bubble is anchored.
gfx::Point GetAnchorPointInScreen(aura::Window* root_window,
                                  ShelfAlignment shelf_alignment) {
  gfx::Rect work_area = GetWorkAreaForBubble(root_window);

  switch (shelf_alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return base::i18n::IsRTL() ? work_area.bottom_right()
                                 : work_area.bottom_left();
    case ShelfAlignment::kLeft:
      return work_area.origin();
    case ShelfAlignment::kRight:
      return work_area.top_right();
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

AppListBubbleView::AppListBubbleView(AppListViewDelegate* view_delegate,
                                     aura::Window* root_window,
                                     ShelfAlignment shelf_alignment)
    : view_delegate_(view_delegate) {
  DCHECK(view_delegate);
  DCHECK(root_window);
  // The bubble is anchored to a screen corner point, but the API takes a rect.
  SetAnchorRect(gfx::Rect(GetAnchorPointInScreen(root_window, shelf_alignment),
                          gfx::Size()));
  SetArrow(GetArrowCorner(shelf_alignment));

  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_parent_window(
      Shell::GetContainer(root_window, kShellWindowId_AppListContainer));

  // Match the system tray bubble radius.
  set_corner_radius(kUnifiedTrayCornerRadius);

  // Remove the default margins so the content fills the bubble.
  set_margins(gfx::Insets());

  // TODO(https://crbug.com/1218229): Add background blur. See TrayBubbleView
  // and BubbleBorder.
  set_color(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kOpaque));

  // Arrow left/right and up/down triggers the same focus movement as
  // tab/shift+tab.
  SetEnableArrowKeyTraversal(true);

  auto* layout = SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  search_box_view_ = AddChildView(std::make_unique<SearchBoxView>(
      /*delegate=*/this, view_delegate, /*app_list_view=*/nullptr));
  SearchBoxViewBase::InitParams params;
  // Show the assistant button until the user types text.
  params.show_close_button_when_active = false;
  params.create_background = false;
  search_box_view_->Init(params);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));

  // NOTE: Passing drag and drop host from a specific shelf instance assumes
  // that the `apps_page_` will not get reused for showing the app list in
  // another root window.
  apps_page_ = AddChildView(std::make_unique<AppListBubbleAppsPage>(
      view_delegate, Shelf::ForWindow(root_window)
                         ->shelf_widget()
                         ->GetDragAndDropHostForAppList()));

  search_page_ = AddChildView(std::make_unique<AppListBubbleSearchPage>(
      view_delegate, search_box_view_));
  search_page_->SetVisible(false);

  assistant_page_ = AddChildView(std::make_unique<AppListBubbleAssistantPage>(
      view_delegate->GetAssistantViewDelegate()));
  assistant_page_->SetVisible(false);
}

AppListBubbleView::~AppListBubbleView() = default;

bool AppListBubbleView::Back() {
  if (search_box_view_->HasSearch()) {
    search_box_view_->ClearSearchAndDeactivateSearchBox();
    return true;
  }
  // TODO(https://crbug.com/1220808): Handle back action for open folders in
  // AppListBubble

  return false;
}

void AppListBubbleView::FocusSearchBox() {
  DCHECK(GetWidget());
  search_box_view_->SetSearchBoxActive(true, /*event_type=*/ui::ET_UNKNOWN);
}

bool AppListBubbleView::IsShowingEmbeddedAssistantUI() const {
  return assistant_page_->GetVisible();
}

void AppListBubbleView::ShowEmbeddedAssistantUI() {
  // The assistant has its own text input field.
  search_box_view_->SetVisible(false);

  apps_page_->SetVisible(false);
  search_page_->SetVisible(false);
  assistant_page_->SetVisible(true);
  assistant_page_->RequestFocus();
}

bool AppListBubbleView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
    case ui::VKEY_BROWSER_BACK:
      // If the ContentsView does not handle the back action, then this is the
      // top level, so we close the app list.
      if (!Back())
        view_delegate_->DismissAppList();
      break;
    default:
      NOTREACHED();
      return false;
  }

  // Don't let the accelerator propagate any further.
  return true;
}

gfx::Size AppListBubbleView::CalculatePreferredSize() const {
  int height = kDefaultHeight - margins().height();
  int width = kDefaultWidth - margins().width();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          GetWidget()->GetNativeWindow());
  gfx::Rect work_area = GetWorkAreaForBubble(GetWidget()->GetNativeWindow());

  if (display.bounds().height() < 800) {
    height = work_area.height() - margins().height() -
             ShelfConfig::Get()->shelf_size() - kExtraTopOfScreenSpacing;
  } else if (display.bounds().height() > 1200) {
    // Calculate the height required to fit the contents of the AppListBubble
    // with no scrolling.
    int height_to_fit_all_apps =
        apps_page_->scroll_view()->contents()->bounds().height() +
        search_box_view_->GetPreferredSize().height();

    int max_height =
        (work_area.height() - margins().height() -
         ShelfConfig::Get()->shelf_size() + kExtraTopOfScreenSpacing) /
        2;

    height = base::clamp(height_to_fit_all_apps,
                         kDefaultHeight - margins().height(), max_height);
  }

  return gfx::Size(width, height);
}

void AppListBubbleView::OnPaint(gfx::Canvas* canvas) {
  // Used to draw/hide the focus bar for the search box view.
  if (search_box_view_->search_box()->HasFocus() &&
      search_box_view_->search_box()->GetText().empty()) {
    PaintFocusBar(
        canvas,
        GetContentsBounds().origin() +
            gfx::Vector2d(0,
                          kUnifiedTrayCornerRadius /*downshift the focus bar*/),
        /*height=*/kSearchBoxIconSize);
  }
  views::View::OnPaint(canvas);
}

void AppListBubbleView::QueryChanged(SearchBoxViewBase* sender) {
  DCHECK_EQ(sender, search_box_view_);
  // TODO(https://crbug.com/1204551): Animated transitions.
  const bool has_search = search_box_view_->HasSearch();
  apps_page_->SetVisible(!has_search);
  search_page_->SetVisible(has_search);
  assistant_page_->SetVisible(false);

  // Ask the controller to start the search.
  std::u16string query = view_delegate_->GetSearchModel()->search_box()->text();
  view_delegate_->StartSearch(query);
  SchedulePaint();
}

void AppListBubbleView::AssistantButtonPressed() {
  ShowEmbeddedAssistantUI();
}

void AppListBubbleView::CloseButtonPressed() {
  // Activate and focus the search box.
  search_box_view_->SetSearchBoxActive(true, /*event_type=*/ui::ET_UNKNOWN);
  search_box_view_->ClearSearch();
}

void AppListBubbleView::OnSearchBoxKeyEvent(ui::KeyEvent* event) {
  // Nothing to do. Search box starts focused, and FocusManager handles arrow
  // key traversal from there.
}

bool AppListBubbleView::CanSelectSearchResults() {
  return search_page_->GetVisible() && search_page_->CanSelectSearchResults();
}

}  // namespace ash
