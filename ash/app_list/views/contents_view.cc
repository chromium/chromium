// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/contents_view.h"

#include <algorithm>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/horizontal_page_container.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_answer_card_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/logging.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// The range of app list transition progress in which the expand arrow'
// opacity changes from 0 to 1.
constexpr float kExpandArrowOpacityStartProgress = 0.61;
constexpr float kExpandArrowOpacityEndProgress = 1;

}  // namespace

ContentsView::ContentsView(AppListView* app_list_view)
    : app_list_view_(app_list_view) {
  pagination_model_.SetTransitionDurations(kPageTransitionDurationInMs,
                                           kOverscrollPageTransitionDurationMs);
  pagination_model_.AddObserver(this);
}

ContentsView::~ContentsView() {
  pagination_model_.RemoveObserver(this);
}

void ContentsView::Init(AppListModel* model) {
  DCHECK(model);
  model_ = model;

  AppListViewDelegate* view_delegate = GetAppListMainView()->view_delegate();

  horizontal_page_container_ = new HorizontalPageContainer(this, model);

  // Add |horizontal_page_container_| as STATE_START corresponding page for
  // fullscreen app list.
  AddLauncherPage(horizontal_page_container_, ash::AppListState::kStateStart);

  // Search results UI.
  search_results_page_view_ = new SearchResultPageView();

  // Search result containers.
  SearchModel::SearchResults* results =
      view_delegate->GetSearchModel()->results();

  if (app_list_features::IsAnswerCardEnabled()) {
    search_result_answer_card_view_ =
        new SearchResultAnswerCardView(view_delegate);
    search_results_page_view_->AddSearchResultContainerView(
        results, search_result_answer_card_view_);
  }

  if (app_list_features::IsNewStyleLauncherEnabled()) {
    expand_arrow_view_ = new ExpandArrowView(this, app_list_view_);
    AddChildView(expand_arrow_view_);
  }

  search_result_tile_item_list_view_ = new SearchResultTileItemListView(
      search_results_page_view_, GetSearchBoxView()->search_box(),
      view_delegate);
  search_results_page_view_->AddSearchResultContainerView(
      results, search_result_tile_item_list_view_);

  search_result_list_view_ =
      new SearchResultListView(GetAppListMainView(), view_delegate);
  search_results_page_view_->AddSearchResultContainerView(
      results, search_result_list_view_);

  AddLauncherPage(search_results_page_view_,
                  ash::AppListState::kStateSearchResults);

  AddLauncherPage(horizontal_page_container_, ash::AppListState::kStateApps);

  int initial_page_index = GetPageIndexForState(ash::AppListState::kStateStart);
  DCHECK_GE(initial_page_index, 0);

  page_before_search_ = initial_page_index;
  // Must only call SetTotalPages once all the launcher pages have been added
  // (as it will trigger a SelectedPageChanged call).
  pagination_model_.SetTotalPages(app_list_pages_.size());

  // Page 0 is selected by SetTotalPages and needs to be 'hidden' when selecting
  // the initial page.
  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  pagination_model_.SelectPage(initial_page_index, false);

  ActivePageChanged();
}

void ContentsView::CancelDrag() {
  if (GetAppsContainerView()->apps_grid_view()->has_dragged_view())
    GetAppsContainerView()->apps_grid_view()->EndDrag(true);
  if (GetAppsContainerView()
          ->app_list_folder_view()
          ->items_grid_view()
          ->has_dragged_view()) {
    GetAppsContainerView()->app_list_folder_view()->items_grid_view()->EndDrag(
        true);
  }
}

void ContentsView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  GetAppsContainerView()->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
}

void ContentsView::SetActiveState(ash::AppListState state) {
  SetActiveState(state, !AppListView::ShortAnimationsForTesting());
}

void ContentsView::SetActiveState(ash::AppListState state, bool animate) {
  if (IsStateActive(state))
    return;

  SetActiveStateInternal(GetPageIndexForState(state), false, animate);
}

int ContentsView::GetActivePageIndex() const {
  // The active page is changed at the beginning of an animation, not the end.
  return pagination_model_.SelectedTargetPage();
}

ash::AppListState ContentsView::GetActiveState() const {
  return GetStateForPageIndex(GetActivePageIndex());
}

bool ContentsView::IsStateActive(ash::AppListState state) const {
  int active_page_index = GetActivePageIndex();
  return active_page_index >= 0 &&
         GetPageIndexForState(state) == active_page_index;
}

int ContentsView::GetPageIndexForState(ash::AppListState state) const {
  // Find the index of the view corresponding to the given state.
  std::map<ash::AppListState, int>::const_iterator it =
      state_to_view_.find(state);
  if (it == state_to_view_.end())
    return -1;

  return it->second;
}

ash::AppListState ContentsView::GetStateForPageIndex(int index) const {
  std::map<int, ash::AppListState>::const_iterator it =
      view_to_state_.find(index);
  if (it == view_to_state_.end())
    return ash::AppListState::kInvalidState;

  return it->second;
}

int ContentsView::NumLauncherPages() const {
  return pagination_model_.total_pages();
}

AppsContainerView* ContentsView::GetAppsContainerView() {
  return horizontal_page_container_->apps_container_view();
}

void ContentsView::SetActiveStateInternal(int page_index,
                                          bool show_search_results,
                                          bool animate) {
  if (!GetPageView(page_index)->visible())
    return;

  if (!show_search_results)
    page_before_search_ = page_index;

  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  // Start animating to the new page.
  pagination_model_.SelectPage(page_index, animate);
  ActivePageChanged();

  if (!animate)
    Layout();
}

void ContentsView::ActivePageChanged() {
  ash::AppListState state = ash::AppListState::kInvalidState;

  std::map<int, ash::AppListState>::const_iterator it =
      view_to_state_.find(GetActivePageIndex());
  if (it != view_to_state_.end())
    state = it->second;

  app_list_pages_[GetActivePageIndex()]->OnWillBeShown();

  GetAppListMainView()->model()->SetState(state);

  UpdateExpandArrowFocusBehavior(state);
}

void ContentsView::ShowSearchResults(bool show) {
  int search_page =
      GetPageIndexForState(ash::AppListState::kStateSearchResults);
  DCHECK_GE(search_page, 0);

  // Search results page is hidden when it is behind the search box, so reshow
  // it here.
  if (show)
    GetPageView(search_page)->SetVisible(true);

  SetActiveStateInternal(show ? search_page : page_before_search_, show, true);
}

bool ContentsView::IsShowingSearchResults() const {
  return IsStateActive(ash::AppListState::kStateSearchResults);
}

void ContentsView::UpdatePageBounds() {
  // The bounds calculations will potentially be mid-transition (depending on
  // the state of the PaginationModel).
  int current_page = std::max(0, pagination_model_.selected_page());
  int target_page = current_page;
  double progress = 1;
  if (pagination_model_.has_transition()) {
    const PaginationModel::Transition& transition =
        pagination_model_.transition();
    if (pagination_model_.is_valid_page(transition.target_page)) {
      target_page = transition.target_page;
      progress = transition.progress;
    }
  }

  ash::AppListState current_state = GetStateForPageIndex(current_page);
  ash::AppListState target_state = GetStateForPageIndex(target_page);

  // Update app list pages.
  for (AppListPage* page : app_list_pages_) {
    gfx::Rect to_rect = page->GetPageBoundsForState(target_state);
    gfx::Rect from_rect = page->GetPageBoundsForState(current_state);

    // Animate linearly (the PaginationModel handles easing).
    gfx::Rect bounds(
        gfx::Tween::RectValueBetween(progress, from_rect, to_rect));

    page->SetBoundsRect(bounds);

    if (ShouldLayoutPage(page, current_state, target_state))
      page->OnAnimationUpdated(progress, current_state, target_state);
  }

  // Update the search box.
  UpdateSearchBox(progress, current_state, target_state);

  // Update the expand arrow view's opacity.
  UpdateExpandArrowOpacity(progress, current_state, target_state);
}

void ContentsView::UpdateSearchBox(double progress,
                                   ash::AppListState current_state,
                                   ash::AppListState target_state) {
  SearchBoxView* search_box = GetSearchBoxView();
  if (!search_box->GetWidget())
    return;

  AppListPage* from_page = GetPageView(GetPageIndexForState(current_state));
  AppListPage* to_page = GetPageView(GetPageIndexForState(target_state));

  gfx::Rect search_box_from(from_page->GetSearchBoxBounds());
  gfx::Rect search_box_to(to_page->GetSearchBoxBounds());
  gfx::Rect search_box_rect =
      gfx::Tween::RectValueBetween(progress, search_box_from, search_box_to);

  search_box->UpdateLayout(progress, current_state, target_state);
  search_box->UpdateBackground(progress, current_state, target_state);
  search_box->GetWidget()->SetBounds(
      search_box->GetViewBoundsForSearchBoxContentsBounds(
          ConvertRectToWidgetWithoutTransform(search_box_rect)));
}

void ContentsView::UpdateExpandArrowOpacity(double progress,
                                            ash::AppListState current_state,
                                            ash::AppListState target_state) {
  if (!expand_arrow_view_)
    return;

  // Don't show |expand_arrow_view_| when the home launcher gestures are
  // disabled in tablet mode.
  if (app_list_view_->IsHomeLauncherEnabledInTabletMode() &&
      !app_list_features::IsHomeLauncherGesturesEnabled()) {
    expand_arrow_view_->layer()->SetOpacity(0);
    return;
  }

  if (current_state == ash::AppListState::kStateSearchResults &&
      (target_state == ash::AppListState::kStateStart ||
       target_state == ash::AppListState::kStateApps)) {
    // Fade in the expand arrow when search results page is opened.
    expand_arrow_view_->layer()->SetOpacity(
        gfx::Tween::FloatValueBetween(progress, 0, 1));
  } else if (target_state == ash::AppListState::kStateSearchResults &&
             (current_state == ash::AppListState::kStateStart ||
              current_state == ash::AppListState::kStateApps)) {
    // Fade out the expand arrow when search results page is closed.
    expand_arrow_view_->layer()->SetOpacity(
        gfx::Tween::FloatValueBetween(progress, 1, 0));
  }
}

void ContentsView::UpdateExpandArrowFocusBehavior(
    ash::AppListState current_state) {
  if (!expand_arrow_view_)
    return;

  if (current_state == ash::AppListState::kStateStart) {
    // The expand arrow is only focusable and has InkDropMode on in peeking
    // state.
    expand_arrow_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    expand_arrow_view_->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
    return;
  }
  expand_arrow_view_->SetInkDropMode(views::InkDropHostView::InkDropMode::OFF);
  expand_arrow_view_->SetFocusBehavior(FocusBehavior::NEVER);
}

PaginationModel* ContentsView::GetAppsPaginationModel() {
  return GetAppsContainerView()->apps_grid_view()->pagination_model();
}

void ContentsView::ShowFolderContent(AppListFolderItem* item) {
  GetAppsContainerView()->ShowActiveFolder(item);
}

AppListPage* ContentsView::GetPageView(int index) const {
  DCHECK_GT(static_cast<int>(app_list_pages_.size()), index);
  return app_list_pages_[index];
}

SearchBoxView* ContentsView::GetSearchBoxView() const {
  return GetAppListMainView()->search_box_view();
}

AppListMainView* ContentsView::GetAppListMainView() const {
  return app_list_view_->app_list_main_view();
}

int ContentsView::AddLauncherPage(AppListPage* view) {
  view->set_contents_view(this);
  AddChildView(view);
  app_list_pages_.push_back(view);
  return app_list_pages_.size() - 1;
}

int ContentsView::AddLauncherPage(AppListPage* view, ash::AppListState state) {
  int page_index = AddLauncherPage(view);
  bool success =
      state_to_view_.insert(std::make_pair(state, page_index)).second;
  success = success &&
            view_to_state_.insert(std::make_pair(page_index, state)).second;

  // There shouldn't be duplicates in either map.
  DCHECK(success);
  return page_index;
}

gfx::Rect ContentsView::GetDefaultSearchBoxBounds() const {
  gfx::Rect search_box_bounds;
  search_box_bounds.set_size(GetSearchBoxView()->GetPreferredSize());
  search_box_bounds.Offset((bounds().width() - search_box_bounds.width()) / 2,
                           0);
  search_box_bounds.set_y(
      AppListConfig::instance().search_box_fullscreen_top_padding());
  return search_box_bounds;
}

gfx::Rect ContentsView::GetSearchBoxBoundsForState(
    ash::AppListState state) const {
  AppListPage* page = GetPageView(GetPageIndexForState(state));
  return page->GetSearchBoxBoundsForState(state);
}

gfx::Rect ContentsView::GetDefaultContentsBounds() const {
  return GetContentsBounds();
}

gfx::Size ContentsView::GetMaximumContentsSize() const {
  int max_width = 0;
  int max_height = 0;
  for (AppListPage* page : app_list_pages_) {
    const gfx::Size size(page->GetPreferredSize());
    max_width = std::max(size.width(), max_width);
    max_height = std::max(size.height(), max_height);
  }
  return gfx::Size(max_width, max_height);
}

bool ContentsView::Back() {
  // If the virtual keyboard is visible, dismiss the keyboard and return early
  auto* const keyboard_controller = keyboard::KeyboardController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    keyboard_controller->HideKeyboardByUser();
    return true;
  }
  ash::AppListState state = view_to_state_[GetActivePageIndex()];
  switch (state) {
    case ash::AppListState::kStateStart:
      // Close the app list when Back() is called from the start page.
      return false;
    case ash::AppListState::kStateApps: {
      PaginationModel* pagination_model =
          GetAppsContainerView()->apps_grid_view()->pagination_model();
      if (GetAppsContainerView()->IsInFolderView()) {
        GetAppsContainerView()->app_list_folder_view()->CloseFolderPage();
      } else if (app_list_view_->IsHomeLauncherEnabledInTabletMode() &&
                 pagination_model->total_pages() > 0 &&
                 pagination_model->selected_page() > 0) {
        pagination_model->SelectPage(
            0, !app_list_view_->ShortAnimationsForTesting());
      } else {
        // Close the app list when Back() is called from the apps page.
        return false;
      }
      break;
    }
    case ash::AppListState::kStateSearchResults:
      GetSearchBoxView()->ClearSearch();
      GetSearchBoxView()->SetSearchBoxActive(false, ui::ET_UNKNOWN);
      ShowSearchResults(false);
      break;
    case ash::AppListState::kStateCustomLauncherPageDeprecated:
    case ash::AppListState::kInvalidState:  // Falls through.
      NOTREACHED();
      break;
  }
  return true;
}

gfx::Size ContentsView::GetDefaultContentsSize() const {
  return horizontal_page_container_->GetPreferredSize();
}

gfx::Size ContentsView::CalculatePreferredSize() const {
  // If shelf is set auto-hide, the work area will become fullscreen. The bottom
  // row of apps will be partially blocked by the shelf when it becomes shown.
  // So always cut the shelf bounds from widget bounds.
  gfx::Size size = GetWidget()->GetNativeView()->bounds().size();
  if (!app_list_view_->is_side_shelf())
    size.set_height(size.height() - AppListConfig::instance().shelf_height());
  return size;
}

void ContentsView::Layout() {
  const gfx::Rect rect = GetContentsBounds();
  if (rect.IsEmpty())
    return;

  if (expand_arrow_view_) {
    // Layout expand arrow.
    gfx::Rect arrow_rect(GetContentsBounds());
    const gfx::Size arrow_size(expand_arrow_view_->GetPreferredSize());
    arrow_rect.set_height(arrow_size.height());
    arrow_rect.ClampToCenteredSize(arrow_size);
    expand_arrow_view_->SetBoundsRect(arrow_rect);
    expand_arrow_view_->SchedulePaint();
  }

  UpdatePageBounds();
}

const char* ContentsView::GetClassName() const {
  return "ContentsView";
}

void ContentsView::TotalPagesChanged() {}

void ContentsView::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0)
    app_list_pages_[old_selected]->OnHidden();

  if (new_selected >= 0)
    app_list_pages_[new_selected]->OnShown();
}

void ContentsView::TransitionStarted() {}

void ContentsView::TransitionChanged() {
  UpdatePageBounds();
}

void ContentsView::TransitionEnded() {}

views::View* ContentsView::GetSelectedView() const {
  return app_list_pages_[GetActivePageIndex()]->GetSelectedView();
}

void ContentsView::UpdateYPositionAndOpacity() {
  AppListViewState state = app_list_view_->app_list_state();
  if (state == AppListViewState::CLOSED ||
      state == AppListViewState::FULLSCREEN_SEARCH ||
      state == AppListViewState::HALF) {
    return;
  }

  if (expand_arrow_view_) {
    const bool should_restore_opacity =
        !app_list_view_->is_in_drag() &&
        (app_list_view_->app_list_state() != AppListViewState::CLOSED);

    // Changes the opacity of expand arrow between 0 and 1 when app list
    // transition progress changes between |kExpandArrowOpacityStartProgress|
    // and |kExpandArrowOpacityEndProgress|.
    expand_arrow_view_->layer()->SetOpacity(
        should_restore_opacity
            ? 1.0f
            : std::min(
                  std::max((app_list_view_->GetAppListTransitionProgress() -
                            kExpandArrowOpacityStartProgress) /
                               (kExpandArrowOpacityEndProgress -
                                kExpandArrowOpacityStartProgress),
                           0.f),
                  1.0f));

    expand_arrow_view_->SchedulePaint();
  }

  AppsContainerView* apps_container_view = GetAppsContainerView();
  SearchBoxView* search_box = GetSearchBoxView();
  search_box->GetWidget()->SetBounds(
      search_box->GetViewBoundsForSearchBoxContentsBounds(
          ConvertRectToWidgetWithoutTransform(
              apps_container_view->GetSearchBoxExpectedBounds())));

  search_results_page_view()->SetBoundsRect(
      apps_container_view->GetSearchBoxExpectedBounds());

  apps_container_view->UpdateYPositionAndOpacity();
}

bool ContentsView::ShouldLayoutPage(AppListPage* page,
                                    ash::AppListState current_state,
                                    ash::AppListState target_state) const {
  if (page == horizontal_page_container_) {
    return (current_state == ash::AppListState::kStateStart &&
            target_state == ash::AppListState::kStateApps) ||
           (current_state == ash::AppListState::kStateApps &&
            target_state == ash::AppListState::kStateStart);
  }

  if (page == search_results_page_view_) {
    return ((current_state == ash::AppListState::kStateSearchResults &&
             target_state == ash::AppListState::kStateStart) ||
            (current_state == ash::AppListState::kStateStart &&
             target_state == ash::AppListState::kStateSearchResults)) ||
           ((current_state == ash::AppListState::kStateSearchResults &&
             target_state == ash::AppListState::kStateApps) ||
            (current_state == ash::AppListState::kStateApps &&
             target_state == ash::AppListState::kStateSearchResults));
  }

  return false;
}

gfx::Rect ContentsView::ConvertRectToWidgetWithoutTransform(
    const gfx::Rect& rect) {
  gfx::Rect widget_rect = rect;
  for (const views::View* v = this; v; v = v->parent()) {
    widget_rect.Offset(v->GetMirroredPosition().OffsetFromOrigin());
  }
  return widget_rect;
}

}  // namespace app_list
