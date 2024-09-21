// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_main_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/search_box/search_box_view_base.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// AppListMainView:

AppListMainView::AppListMainView(AppListViewDelegate* delegate,
                                 AppListView* app_list_view)
    : delegate_(delegate), app_list_view_(app_list_view) {
  // We need a layer to apply transform to in small display so that the apps
  // grid fits in the display.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetUseDefaultFillLayout(true);
}

AppListMainView::~AppListMainView() = default;

void AppListMainView::Init(int initial_apps_page,
                           SearchBoxView* search_box_view) {
  search_box_view_ = search_box_view;
  AddContentsViews();

  // Switch the apps grid view to the specified page.
  PaginationModel* pagination_model = GetAppsPaginationModel();
  if (pagination_model->is_valid_page(initial_apps_page))
    pagination_model->SelectPage(initial_apps_page, false);
}

void AppListMainView::AddContentsViews() {
  DCHECK(search_box_view_);
  auto contents_view = std::make_unique<ContentsView>(app_list_view_);
  contents_view->Init();
  contents_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  contents_view->layer()->SetMasksToBounds(true);
  contents_view_ = AddChildView(std::move(contents_view));
}

void AppListMainView::ShowAppListWhenReady() {
  // After switching to tablet mode, other app windows may be active. Show the
  // app list without activating it to avoid breaking other windows' state.
  const aura::Window* active_window =
      wm::GetActivationClient(
          app_list_view_->GetWidget()->GetNativeView()->GetRootWindow())
          ->GetActiveWindow();
  if (active_window)
    GetWidget()->ShowInactive();
  else
    GetWidget()->Show();
}

PaginationModel* AppListMainView::GetAppsPaginationModel() {
  return contents_view_->apps_container_view()
      ->apps_grid_view()
      ->pagination_model();
}

void AppListMainView::QueryChanged(const std::u16string& trimmed_query,
                                   bool initiated_by_user) {
  app_list_view_->SetStateFromSearchBoxView(trimmed_query.empty(),
                                            initiated_by_user);
  contents_view_->ShowSearchResults(search_box_view_->is_search_box_active() ||
                                    !trimmed_query.empty());
  contents_view_->search_result_page_view()->UpdateForNewSearch();
}

void AppListMainView::ActiveChanged(SearchBoxViewBase* sender) {
  // Do not update views on closing.
  if (app_list_view_->app_list_state() == AppListViewState::kClosed) {
    return;
  }

  if (search_box_view_->is_search_box_active()) {
    // Show zero state suggestions when search box is activated with an empty
    // query.
    const bool is_query_empty = sender->IsSearchBoxTrimmedQueryEmpty();
    app_list_view_->SetStateFromSearchBoxView(
        is_query_empty, true /*triggered_by_contents_change*/);
    contents_view_->ShowSearchResults(true);
  } else {
    // Close the search results page if the search box is inactive.
    contents_view_->ShowSearchResults(false);
  }
}

void AppListMainView::OnSearchBoxKeyEvent(ui::KeyEvent* event) {
  app_list_view_->RedirectKeyEventToSearchBox(event);

  if (!IsUnhandledUpDownKeyEvent(*event)) {
    return;
  }

  // Handles arrow key events from the search box while the search box is
  // inactive. This covers both folder traversal and apps grid traversal. Search
  // result traversal is handled in |HandleKeyEvent|
  AppListPage* page =
      contents_view_->GetPageView(contents_view_->GetActivePageIndex());
  views::View* next_view = nullptr;

  if (event->key_code() == ui::VKEY_UP) {
    next_view = page->GetLastFocusableView();
  } else {
    next_view = page->GetFirstFocusableView();
  }

  if (next_view) {
    next_view->RequestFocus();
  }
  event->SetHandled();
}

bool AppListMainView::CanSelectSearchResults() {
  // If there's a result, keyboard selection is allowed.
  return !!contents_view_->search_result_page_view()->CanSelectSearchResults();
}

void AppListMainView::AssistantButtonPressed() {
  delegate_->StartAssistant(
      assistant::AssistantEntryPoint::kLauncherSearchBoxIcon);
}

void AppListMainView::CloseButtonPressed() {
  // Deactivate the search box.
  search_box_view_->SetSearchBoxActive(false, ui::EventType::kUnknown);
  search_box_view_->ClearSearch();
}

BEGIN_METADATA(AppListMainView)
END_METADATA

}  // namespace ash
