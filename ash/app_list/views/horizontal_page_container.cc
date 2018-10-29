// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/horizontal_page_container.h"

#include "ash/app_list/pagination_controller.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/views/controls/label.h"

namespace app_list {

HorizontalPageContainer::HorizontalPageContainer(ContentsView* contents_view,
                                                 AppListModel* model)
    : contents_view_(contents_view) {
  pagination_model_.SetTransitionDurations(kPageTransitionDurationInMs,
                                           kOverscrollPageTransitionDurationMs);
  pagination_model_.AddObserver(this);
  pagination_controller_ = std::make_unique<PaginationController>(
      &pagination_model_, PaginationController::SCROLL_AXIS_HORIZONTAL);

  // Add horizontal pages.
  apps_container_view_ = new AppsContainerView(contents_view_, model);
  AddHorizontalPage(apps_container_view_);
  pagination_model_.SetTotalPages(horizontal_pages_.size());

  // By default select apps container page.
  pagination_model_.SelectPage(GetIndexForPage(apps_container_view_), false);
}

HorizontalPageContainer::~HorizontalPageContainer() {
  pagination_model_.RemoveObserver(this);
}

gfx::Size HorizontalPageContainer::CalculatePreferredSize() const {
  if (!GetWidget())
    return gfx::Size();

  return contents_view_->GetPreferredSize();
}

void HorizontalPageContainer::Layout() {
  if (!GetWidget())
    return;

  for (size_t i = 0; i < horizontal_pages_.size(); ++i) {
    HorizontalPage* page = horizontal_pages_[i];
    gfx::Rect page_bounds(
        page->GetPageBoundsForState(contents_view_->GetActiveState()));
    page_bounds.Offset(GetOffsetForPageIndex(i));
    page->SetBoundsRect(page_bounds);
  }
}

void HorizontalPageContainer::OnGestureEvent(ui::GestureEvent* event) {
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds()))
    event->SetHandled();
}

void HorizontalPageContainer::OnWillBeHidden() {
  GetSelectedPage()->OnWillBeHidden();
}

void HorizontalPageContainer::OnAnimationUpdated(double progress,
                                                 ash::AppListState from_state,
                                                 ash::AppListState to_state) {
  for (size_t i = 0; i < horizontal_pages_.size(); ++i) {
    HorizontalPage* page = horizontal_pages_[i];
    gfx::Rect to_rect = page->GetPageBoundsForState(to_state);
    gfx::Rect from_rect = page->GetPageBoundsForState(from_state);

    // Animate linearly (the PaginationModel handles easing).
    gfx::Rect bounds(
        gfx::Tween::RectValueBetween(progress, from_rect, to_rect));
    bounds.Offset(GetOffsetForPageIndex(i));
    page->SetBoundsRect(bounds);
  }
}

gfx::Rect HorizontalPageContainer::GetSearchBoxBounds() const {
  return GetSearchBoxBoundsForState(contents_view_->GetActiveState());
}

gfx::Rect HorizontalPageContainer::GetSearchBoxBoundsForState(
    ash::AppListState state) const {
  // The search box bounds are decided by AppsContainerView and are not changed
  // during horizontal page switching.
  return apps_container_view_->GetSearchBoxExpectedBounds();
}

gfx::Rect HorizontalPageContainer::GetPageBoundsForState(
    ash::AppListState state) const {
  gfx::Rect onscreen_bounds = GetDefaultContentsBounds();

  // Both STATE_START and STATE_APPS are AppsContainerView page.
  if (state == ash::AppListState::kStateApps ||
      state == ash::AppListState::kStateStart) {
    return onscreen_bounds;
  }

  return GetBelowContentsOffscreenBounds(onscreen_bounds.size());
}

views::View* HorizontalPageContainer::GetFirstFocusableView() {
  return GetSelectedPage()->GetFirstFocusableView();
}

views::View* HorizontalPageContainer::GetLastFocusableView() {
  return GetSelectedPage()->GetLastFocusableView();
}

bool HorizontalPageContainer::ShouldShowSearchBox() const {
  return GetSelectedPage()->ShouldShowSearchBox();
}

void HorizontalPageContainer::TotalPagesChanged() {}

void HorizontalPageContainer::SelectedPageChanged(int old_selected,
                                                  int new_selected) {
  Layout();
}

void HorizontalPageContainer::TransitionStarted() {
  Layout();
}

void HorizontalPageContainer::TransitionChanged() {
  Layout();

  // Transition the search box opacity.
  const int current_page = pagination_model_.selected_page();
  DCHECK(pagination_model_.is_valid_page(current_page));
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  const bool is_valid = pagination_model_.is_valid_page(transition.target_page);
  float search_box_opacity =
      GetSelectedPage()->ShouldShowSearchBox() ? 1.0f : 0.0f;
  if (is_valid) {
    float target_search_box_opacity =
        horizontal_pages_[transition.target_page]->ShouldShowSearchBox() ? 1.0f
                                                                         : 0.0f;
    search_box_opacity = gfx::Tween::FloatValueBetween(
        transition.progress, search_box_opacity, target_search_box_opacity);
  }
  contents_view_->GetSearchBoxView()->layer()->SetOpacity(search_box_opacity);
  contents_view_->search_results_page_view()->layer()->SetOpacity(
      search_box_opacity);
}

void HorizontalPageContainer::TransitionEnded() {
  Layout();
}

int HorizontalPageContainer::AddHorizontalPage(HorizontalPage* view) {
  AddChildView(view);
  horizontal_pages_.emplace_back(view);
  return horizontal_pages_.size() - 1;
}

int HorizontalPageContainer::GetIndexForPage(HorizontalPage* view) const {
  for (size_t i = 0; i < horizontal_pages_.size(); ++i) {
    if (horizontal_pages_[i] == view)
      return i;
  }
  return -1;
}

HorizontalPage* HorizontalPageContainer::GetSelectedPage() {
  return const_cast<HorizontalPage*>(
      const_cast<const HorizontalPageContainer*>(this)->GetSelectedPage());
}

const HorizontalPage* HorizontalPageContainer::GetSelectedPage() const {
  const int current_page = pagination_model_.selected_page();
  DCHECK(pagination_model_.is_valid_page(current_page));
  return horizontal_pages_[current_page];
}

gfx::Vector2d HorizontalPageContainer::GetOffsetForPageIndex(int index) const {
  const int current_page = pagination_model_.selected_page();
  DCHECK(pagination_model_.is_valid_page(current_page));
  const PaginationModel::Transition& transition =
      pagination_model_.transition();
  const bool is_valid = pagination_model_.is_valid_page(transition.target_page);
  const int dir = transition.target_page > current_page ? -1 : 1;
  int x_offset = 0;
  // TODO(https://crbug.com/820510): figure out the right pagination style.
  if (index < current_page)
    x_offset = -width();
  else if (index > current_page)
    x_offset = width();

  if (is_valid) {
    if (index == current_page || index == transition.target_page) {
      x_offset += transition.progress * width() * dir;
    }
  }
  return gfx::Vector2d(x_offset, 0);
}

}  // namespace app_list
