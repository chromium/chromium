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
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

AppListBubbleView::AppListBubbleView(
    AppListViewDelegate* view_delegate,
    ApplicationDragAndDropHost* drag_and_drop_host)
    : view_delegate_(view_delegate) {
  DCHECK(view_delegate);
  DCHECK(drag_and_drop_host);

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
      view_delegate, drag_and_drop_host));

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

int AppListBubbleView::GetHeightToFitAllApps() const {
  return apps_page_->scroll_view()->contents()->bounds().height() +
         search_box_view_->GetPreferredSize().height();
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
