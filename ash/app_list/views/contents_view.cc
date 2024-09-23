// Copyright 2012 The Chromium Authors
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
#include "ash/app_list/views/assistant/assistant_page_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
// The preferred search box height.
constexpr int kSearchBoxHeight = 48;

// The top search box margin (measured from the app list view top bound) when
// app list view is in peeking state on non-apps page.
constexpr int kDefaultSearchBoxTopMarginInPeekingState = 24;

// The top search box margin (measured from the app list view top bound) when
// app list view is in peeking state on the apps page.
constexpr int kSearchBoxTopMarginInPeekingAppsPage = 84;
constexpr int kSearchBarMinWidth = 440;

// Duration for page transition.
constexpr base::TimeDelta kPageTransitionDuration = base::Milliseconds(250);

// Duration for overscroll page transition.
constexpr base::TimeDelta kOverscrollPageTransitionDuration =
    base::Milliseconds(50);

}  // namespace

ContentsView::ContentsView(AppListView* app_list_view)
    : app_list_view_(app_list_view) {
  pagination_model_.SetTransitionDurations(kPageTransitionDuration,
                                           kOverscrollPageTransitionDuration);
  pagination_model_.AddObserver(this);
}

ContentsView::~ContentsView() {
  pagination_model_.RemoveObserver(this);
}

// static
int ContentsView::GetPeekingSearchBoxTopMarginOnPage(AppListState page) {
  return page == AppListState::kStateApps
             ? kSearchBoxTopMarginInPeekingAppsPage
             : kDefaultSearchBoxTopMarginInPeekingState;
}

void ContentsView::Init() {
  AppListViewDelegate* view_delegate = GetAppListMainView()->view_delegate();
  apps_container_view_ = AddLauncherPage(
      std::make_unique<AppsContainerView>(this), AppListState::kStateApps);

  // Search results UI.
  auto search_result_page_view = std::make_unique<SearchResultPageView>();
  search_result_page_view->InitializeContainers(view_delegate,
                                                GetSearchBoxView());

  search_result_page_view_ = AddLauncherPage(std::move(search_result_page_view),
                                             AppListState::kStateSearchResults);

  auto assistant_page_view = std::make_unique<AssistantPageView>(
      view_delegate->GetAssistantViewDelegate());
  assistant_page_view->SetVisible(false);
  assistant_page_view_ = AddLauncherPage(std::move(assistant_page_view),
                                         AppListState::kStateEmbeddedAssistant);

  int initial_page_index = GetPageIndexForState(AppListState::kStateApps);
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

  // Hide the search results initially.
  ShowSearchResults(false);
}

void ContentsView::ResetForShow() {
  apps_container_view_->ResetForShowApps();
  // SearchBoxView::ResetForShow() before SetActiveState(). It clears the search
  // query internally, which can show the search results page through
  // QueryChanged(). Since it wants to reset to kStateApps, first reset the
  // search box and then set its active state to kStateApps.
  GetSearchBoxView()->ResetForShow();
  // Make sure the default visibilities of the pages. This should be done before
  // SetActiveState() since it checks the visibility of the pages.
  apps_container_view_->SetVisible(true);
  search_result_page_view_->SetVisible(false);
  if (assistant_page_view_)
    assistant_page_view_->SetVisible(false);
  SetActiveState(AppListState::kStateApps, /*animate=*/false);
}

void ContentsView::CancelDrag() {
  if (apps_container_view_->apps_grid_view()->has_dragged_item())
    apps_container_view_->apps_grid_view()->EndDrag(true);
  if (apps_container_view_->app_list_folder_view()
          ->items_grid_view()
          ->has_dragged_item()) {
    apps_container_view_->app_list_folder_view()->items_grid_view()->EndDrag(
        true);
  }
}

void ContentsView::OnAppListViewTargetStateChanged(
    AppListViewState target_state) {
  if (target_state == AppListViewState::kClosed) {
    CancelDrag();
    return;
  }
}

void ContentsView::SetActiveState(AppListState state) {
  SetActiveState(state, true /*animate*/);
}

void ContentsView::SetActiveState(AppListState state, bool animate) {
  if (IsStateActive(state))
    return;

  // The primary way to set the state to search or Assistant results should be
  // via |ShowSearchResults| or |ShowEmbeddedAssistantUI|.
  DCHECK(state != AppListState::kStateSearchResults &&
         state != AppListState::kStateEmbeddedAssistant);

  const int page_index = GetPageIndexForState(state);
  page_before_search_ = page_index;
  SetActiveStateInternal(page_index, animate);
}

int ContentsView::GetActivePageIndex() const {
  // The active page is changed at the beginning of an animation, not the end.
  return pagination_model_.SelectedTargetPage();
}

AppListState ContentsView::GetActiveState() const {
  return GetStateForPageIndex(GetActivePageIndex());
}

bool ContentsView::IsStateActive(AppListState state) const {
  int active_page_index = GetActivePageIndex();
  return active_page_index >= 0 &&
         GetPageIndexForState(state) == active_page_index;
}

int ContentsView::GetPageIndexForState(AppListState state) const {
  // Find the index of the view corresponding to the given state.
  std::map<AppListState, int>::const_iterator it = state_to_view_.find(state);
  if (it == state_to_view_.end())
    return -1;

  return it->second;
}

AppListState ContentsView::GetStateForPageIndex(int index) const {
  std::map<int, AppListState>::const_iterator it = view_to_state_.find(index);
  if (it == view_to_state_.end())
    return AppListState::kInvalidState;

  return it->second;
}

int ContentsView::NumLauncherPages() const {
  return pagination_model_.total_pages();
}

gfx::Size ContentsView::AdjustSearchBoxSizeToFitMargins(
    const gfx::Size& preferred_size) const {
  const int padded_width =
      GetContentsBounds().width() - 2 * AppsContainerView::kHorizontalMargin;
  return gfx::Size(
      std::clamp(padded_width, kSearchBarMinWidth, preferred_size.width()),
      preferred_size.height());
}

void ContentsView::SetActiveStateInternal(int page_index, bool animate) {
  if (!GetPageView(page_index)->GetVisible())
    return;

  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  // Start animating to the new page. Disable animation for tests.
  bool should_animate = animate && !set_active_state_without_animation_ &&
                        !ui::ScopedAnimationDurationScaleMode::is_zero();

  // There's a chance of selecting page during the transition animation. To
  // reschedule the new animation from the beginning, |pagination_model_| needs
  // to finish the ongoing animation here.
  if (should_animate && pagination_model_.has_transition() &&
      pagination_model_.transition().target_page != page_index) {
    pagination_model_.FinishAnimation();
    // If the pending animation was animating from the current target page, the
    // target page might have got hidden as the animation was finished. Make
    // sure the page is reshown in that case.
    GetPageView(page_index)->SetVisible(true);
  }
  pagination_model_.SelectPage(page_index, should_animate);
  ActivePageChanged();

  if (!should_animate)
    DeprecatedLayoutImmediately();
}

void ContentsView::ActivePageChanged() {
  AppListState state = AppListState::kInvalidState;

  std::map<int, AppListState>::const_iterator it =
      view_to_state_.find(GetActivePageIndex());
  if (it != view_to_state_.end())
    state = it->second;

  app_list_pages_[GetActivePageIndex()]->OnWillBeShown();

  GetAppListMainView()->view_delegate()->OnAppListPageChanged(state);
  UpdateSearchBoxVisibility(state);
  app_list_view_->UpdateWindowTitle();
}

void ContentsView::ShowSearchResults(bool show) {
  int search_page = GetPageIndexForState(AppListState::kStateSearchResults);
  DCHECK_GE(search_page, 0);

  // SetVisible() only when showing search results, the search results page will
  // be hidden at the end of its own bounds animation.
  if (show) {
    search_result_page_view()->SetVisible(true);

    // Always to hide `assistant_page_view_` in case it is visible.
    assistant_page_view_->SetVisible(false);

    // `page_before_search_` could be invisible when showing
    // `assistant_page_view_`.
    GetPageView(page_before_search_)->SetVisible(true);
  }

  SetActiveStateInternal(show ? search_page : page_before_search_,
                         true /*animate*/);
  if (show)
    search_result_page_view()->UpdateResultContainersVisibility();
}

bool ContentsView::IsShowingSearchResults() const {
  return IsStateActive(AppListState::kStateSearchResults);
}

void ContentsView::ShowEmbeddedAssistantUI(bool show) {
  const int assistant_page =
      GetPageIndexForState(AppListState::kStateEmbeddedAssistant);
  DCHECK_GE(assistant_page, 0);

  const int current_page = pagination_model_.SelectedTargetPage();
  // When closing the Assistant UI we return to the last page before the
  // search box.
  const int next_page = show ? assistant_page : page_before_search_;

  // Show or hide results.
  if (current_page != next_page) {
    GetPageView(current_page)->SetVisible(false);
    GetPageView(next_page)->SetVisible(true);
  }

  SetActiveStateInternal(next_page, true /*animate*/);
  // Sometimes the page stays in |assistant_page|, but the preferred bounds
  // might change meanwhile.
  if (show && current_page == assistant_page) {
    GetPageView(assistant_page)
        ->UpdatePageBoundsForState(
            AppListState::kStateEmbeddedAssistant, GetContentsBounds(),
            GetSearchBoxBounds(AppListState::kStateEmbeddedAssistant));
  }
  // If |next_page| is kStateApps, we need to set app_list_view to
  // kPeeking and layout the suggestion chips.
  if (next_page == GetPageIndexForState(AppListState::kStateApps)) {
    GetSearchBoxView()->ClearSearch();
    GetSearchBoxView()->SetSearchBoxActive(false, ui::EventType::kUnknown);
    apps_container_view_->DeprecatedLayoutImmediately();
  }
}

bool ContentsView::IsShowingEmbeddedAssistantUI() const {
  return IsStateActive(AppListState::kStateEmbeddedAssistant);
}

void ContentsView::InitializeSearchBoxAnimation(AppListState current_state,
                                                AppListState target_state) {
  SearchBoxView* search_box = GetSearchBoxView();
  if (!search_box->GetWidget())
    return;

  search_box->UpdateLayout(target_state,
                           GetSearchBoxSize(target_state).height());

  gfx::Rect target_bounds = GetSearchBoxBounds(target_state);
  target_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(target_bounds));

  // The search box animation is conducted as transform animation. Initially
  // search box changes its bounds to the target bounds but sets the transform
  // to be original bounds. Note that this transform shouldn't be animated
  // through ui::LayerAnimator since intermediate transformed bounds might not
  // match with other animation and that could look janky.
  search_box->SetBoundsRect(target_bounds);

  UpdateSearchBoxAnimation(0.0f, current_state, target_state);
}

void ContentsView::UpdateSearchBoxAnimation(double progress,
                                            AppListState current_state,
                                            AppListState target_state) {
  SearchBoxView* search_box = GetSearchBoxView();
  if (!search_box->GetWidget())
    return;

  gfx::Rect previous_bounds = GetSearchBoxBounds(current_state);
  previous_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(previous_bounds));
  gfx::Rect target_bounds = GetSearchBoxBounds(target_state);
  target_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(target_bounds));

  gfx::Rect current_bounds =
      gfx::Tween::RectValueBetween(progress, previous_bounds, target_bounds);
  gfx::Transform transform;

  if (current_bounds != target_bounds) {
    transform.Translate(current_bounds.origin() - target_bounds.origin());
    transform.Scale(
        static_cast<float>(current_bounds.width()) / target_bounds.width(),
        static_cast<float>(current_bounds.height()) / target_bounds.height());
  }
  search_box->layer()->SetTransform(transform);

  // Update search box view layer.
  const float current_radius =
      search_box->GetSearchBoxBorderCornerRadiusForState(current_state);
  const float target_radius =
      search_box->GetSearchBoxBorderCornerRadiusForState(target_state);
  search_box->layer()->SetClipRect(search_box->GetContentsBounds());
  search_box->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(
      gfx::Tween::FloatValueBetween(progress, current_radius, target_radius)));
}

void ContentsView::UpdateSearchBoxVisibility(AppListState current_state) {
  // Hide search box widget in order to click on the embedded Assistant UI.
  const bool show_search_box =
      current_state != AppListState::kStateEmbeddedAssistant;
  GetSearchBoxView()->SetVisible(show_search_box);
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

void ContentsView::AddLauncherPageInternal(std::unique_ptr<AppListPage> view,
                                           AppListState state) {
  view->set_contents_view(this);
  app_list_pages_.push_back(AddChildView(std::move(view)));
  int page_index = app_list_pages_.size() - 1;
  bool success =
      state_to_view_.insert(std::make_pair(state, page_index)).second;
  success = success &&
            view_to_state_.insert(std::make_pair(page_index, state)).second;

  // There shouldn't be duplicates in either map.
  DCHECK(success);
}

gfx::Rect ContentsView::GetSearchBoxBounds(AppListState state) const {
  const gfx::Size size = GetSearchBoxSize(state);
  const int top =
      apps_container_view_
          ->CalculateMarginsForAvailableBounds(
              GetContentsBounds(), GetSearchBoxSize(AppListState::kStateApps))
          .top();
  return gfx::Rect(gfx::Point((width() - size.width()) / 2, top), size);
}

gfx::Size ContentsView::GetSearchBoxSize(AppListState state) const {
  AppListPage* page = GetPageView(GetPageIndexForState(state));
  gfx::Size size_preferred_by_page = page->GetPreferredSearchBoxSize();
  if (!size_preferred_by_page.IsEmpty())
    return AdjustSearchBoxSizeToFitMargins(size_preferred_by_page);

  gfx::Size preferred_size = GetSearchBoxView()->GetPreferredSize();

  preferred_size.set_height(kSearchBoxHeight);

  return AdjustSearchBoxSizeToFitMargins(preferred_size);
}

bool ContentsView::Back() {
  // If the virtual keyboard is visible, dismiss the keyboard and return early
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    keyboard_controller->HideKeyboardByUser();
    return true;
  }

  AppListState state = view_to_state_[GetActivePageIndex()];
  switch (state) {
    case AppListState::kStateApps: {
      PaginationModel* pagination_model =
          apps_container_view_->apps_grid_view()->pagination_model();
      if (apps_container_view_->IsInFolderView()) {
        apps_container_view_->app_list_folder_view()->CloseFolderPage();
      } else if (pagination_model->total_pages() > 0 &&
                 pagination_model->selected_page() > 0) {
        bool animate = !ui::ScopedAnimationDurationScaleMode::is_zero();
        pagination_model->SelectPage(0, animate);
      } else {
        return false;
      }
      break;
    }
    case AppListState::kStateSearchResults:
      GetSearchBoxView()->ClearSearchAndDeactivateSearchBox();
      ShowSearchResults(false);
      break;
    case AppListState::kStateEmbeddedAssistant:
      GetAppListMainView()->view_delegate()->EndAssistant(
          assistant::AssistantExitPoint::kBackInLauncher);
      ShowEmbeddedAssistantUI(false);
      break;
    case AppListState::kStateStart_DEPRECATED:
    case AppListState::kInvalidState:
      NOTREACHED();
  }
  return true;
}

void ContentsView::Layout(PassKey) {
  const gfx::Rect rect = GetContentsBounds();
  if (rect.IsEmpty())
    return;

  if (pagination_model_.has_transition())
    return;

  UpdateYPositionAndOpacity();

  const AppListState current_state =
      GetStateForPageIndex(pagination_model_.selected_page());
  SearchBoxView* const search_box = GetSearchBoxView();
  const int search_box_height = GetSearchBoxSize(current_state).height();
  search_box->UpdateLayout(current_state, search_box_height);
  search_box->UpdateBackground(current_state);

  // Reset the transform which can be set through animation
  search_box->layer()->SetTransform(gfx::Transform());
}

void ContentsView::TotalPagesChanged(int previous_page_count,
                                     int new_page_count) {}

void ContentsView::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0)
    app_list_pages_[old_selected]->OnHidden();

  if (new_selected >= 0)
    app_list_pages_[new_selected]->OnShown();
}

void ContentsView::TransitionStarted() {
  const int current_page = pagination_model_.selected_page();
  const int target_page = pagination_model_.transition().target_page;

  const AppListState current_state = GetStateForPageIndex(current_page);
  const AppListState target_state = GetStateForPageIndex(target_page);
  for (AppListPage* page : app_list_pages_)
    page->OnAnimationStarted(current_state, target_state);

  InitializeSearchBoxAnimation(current_state, target_state);
}

void ContentsView::TransitionChanged() {
  const int current_page = pagination_model_.selected_page();
  const int target_page = pagination_model_.transition().target_page;

  const AppListState current_state = GetStateForPageIndex(current_page);
  const AppListState target_state = GetStateForPageIndex(target_page);
  const double progress = pagination_model_.transition().progress;
  for (AppListPage* page : app_list_pages_) {
    if (!page->GetVisible() ||
        !ShouldLayoutPage(page, current_state, target_state)) {
      continue;
    }
    page->OnAnimationUpdated(progress, current_state, target_state);
  }

  // Update search box's transform gradually. See the comment in
  // InitiateSearchBoxAnimation for why it's not animated through
  // ui::LayerAnimator.
  UpdateSearchBoxAnimation(progress, current_state, target_state);
}

void ContentsView::UpdateYPositionAndOpacity() {
  const int current_page = pagination_model_.has_transition()
                               ? pagination_model_.transition().target_page
                               : pagination_model_.selected_page();
  const AppListState current_state = GetStateForPageIndex(current_page);

  // The search box bounds are determined by the apps container internal
  // margins, which depend on the apps container view size and app list config.
  // Make sure the apps container bounds are set before calculating search box
  // bounds, so `apps_container_view_` has up to date AppListConfig when
  // AppsContainerView::CalculateMarginsForAvailableBounds() gets called when
  // calculating search box y position.
  apps_container_view_->SetBoundsRect(GetContentsBounds());

  SearchBoxView* search_box = GetSearchBoxView();
  const gfx::Rect search_box_bounds = GetSearchBoxBounds(current_state);
  const gfx::Rect search_rect =
      search_box->GetViewBoundsForSearchBoxContentsBounds(
          ConvertRectToWidgetWithoutTransform(search_box_bounds));
  search_box->SetBoundsRect(search_rect);

  for (AppListPage* page : app_list_pages_) {
    page->UpdatePageBoundsForState(current_state, GetContentsBounds(),
                                   search_box_bounds);
    page->UpdatePageOpacityForState(current_state, 1.0f);
  }
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
ContentsView::CreateTransitionAnimationSettings(ui::Layer* layer) const {
  DCHECK(pagination_model_.has_transition());
  auto settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetTransitionDuration(
      pagination_model_.GetTransitionAnimationSlideDuration());
  return settings;
}

bool ContentsView::ShouldLayoutPage(AppListPage* page,
                                    AppListState current_state,
                                    AppListState target_state) const {
  if (page == apps_container_view_ || page == search_result_page_view_) {
    return ((current_state == AppListState::kStateSearchResults &&
             target_state == AppListState::kStateApps) ||
            (current_state == AppListState::kStateApps &&
             target_state == AppListState::kStateSearchResults));
  }

  if (page == assistant_page_view_) {
    return current_state == AppListState::kStateEmbeddedAssistant ||
           target_state == AppListState::kStateEmbeddedAssistant;
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

BEGIN_METADATA(ContentsView)
END_METADATA

}  // namespace ash
