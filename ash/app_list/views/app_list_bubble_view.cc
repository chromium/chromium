// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/assistant/app_list_bubble_assistant_page.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
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
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {
namespace {

// Space between the edge of the bubble and the edge of the work area.
constexpr int kWorkAreaPadding = 8;

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

}  // namespace

AppListBubbleView::AppListBubbleView(AppListViewDelegate* view_delegate,
                                     aura::Window* root_window)
    : view_delegate_(view_delegate), root_window_(root_window) {
  DCHECK(view_delegate);
  DCHECK(root_window);

  // Set up rounded corners and background blur, similar to TrayBubbleView.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kUnifiedTrayCornerRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(kUnifiedMenuBackgroundBlur);

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

gfx::Rect AppListBubbleView::GetBubbleBounds() const {
  const gfx::Rect work_area = GetWorkAreaForBubble(root_window_);
  const gfx::Size bubble_size = CalculatePreferredSize();
  const int padding = kWorkAreaPadding;  // Shorten name for readability.
  int x = 0;
  int y = 0;
  switch (Shelf::ForWindow(root_window_)->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      if (base::i18n::IsRTL())
        x = work_area.right() - padding - bubble_size.width();
      else
        x = work_area.x() + padding;
      y = work_area.bottom() - padding - bubble_size.height();
      break;
    case ShelfAlignment::kLeft:
      x = work_area.x() + padding;
      y = work_area.y() + padding;
      break;
    case ShelfAlignment::kRight:
      x = work_area.right() - padding - bubble_size.width();
      y = work_area.y() + padding;
      break;
  }
  return gfx::Rect(x, y, bubble_size.width(), bubble_size.height());
}

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
  const int default_height = 688;
  // As of August 2021 the assistant cards require a minimum width of 640. If
  // the cards become narrower then this could be reduced.
  const int default_width = 640;
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const gfx::Rect work_area = GetWorkAreaForBubble(root_window_);
  int height = default_height;

  // If the work area height is too small to fit the default size bubble, then
  // calculate a smaller height to fit in the work area. Otherwise, if the work
  // area height is tall enough to fit at least two default sized bubbles, then
  // calculate a taller bubble with height taking no more than half the work
  // area.
  if (work_area.height() <
      default_height + shelf_size + kExtraTopOfScreenSpacing) {
    height = work_area.height() - shelf_size - kExtraTopOfScreenSpacing;
  } else if (work_area.height() >
             default_height * 2 + shelf_size + kExtraTopOfScreenSpacing) {
    // Calculate the height required to fit the contents of the AppListBubble
    // with no scrolling.
    int height_to_fit_all_apps =
        apps_page_->scroll_view()->contents()->bounds().height() +
        search_box_view_->GetPreferredSize().height();

    int max_height =
        (work_area.height() - shelf_size - kExtraTopOfScreenSpacing) / 2;

    DCHECK_GE(max_height, default_height);
    height = base::clamp(height_to_fit_all_apps, default_height, max_height);
  }

  return gfx::Size(default_width, height);
}

void AppListBubbleView::OnThemeChanged() {
  views::View::OnThemeChanged();

  layer()->SetColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));
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
