// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_HORIZONTAL_PAGE_CONTAINER_H_
#define ASH_APP_LIST_VIEWS_HORIZONTAL_PAGE_CONTAINER_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/macros.h"

namespace ash {

class AppsContainerView;
class HorizontalPage;
class PaginationController;

// HorizontalPageContainer contains a list of HorizontalPage that are
// horizontally laid out. These pages can be switched with gesture scrolling.
class APP_LIST_EXPORT HorizontalPageContainer
    : public AppListPage,
      public ash::PaginationModelObserver {
 public:
  HorizontalPageContainer(ContentsView* contents_view, AppListModel* model);
  ~HorizontalPageContainer() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  const char* GetClassName() const override;

  // AppListPage overrides:
  void OnWillBeHidden() override;
  void OnAnimationStarted(ash::AppListState from_state,
                          ash::AppListState to_state) override;
  gfx::Rect GetPageBoundsForState(
      ash::AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const override;
  void UpdateOpacityForState(ash::AppListState state) override;
  views::View* GetFirstFocusableView() override;
  views::View* GetLastFocusableView() override;
  bool ShouldShowSearchBox() const override;

  AppsContainerView* apps_container_view() { return apps_container_view_; }

  void OnTabletModeChanged(bool started);

 private:
  // PaginationModelObserver:
  void TotalPagesChanged(int previous_page_count, int new_page_count) override;
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarting() override;
  void TransitionChanged() override;
  void TransitionEnded() override;

  // Adds a horizontal page to this view.
  int AddHorizontalPage(HorizontalPage* view);

  // Gets the index of a horizontal page in |horizontal_pages_|. Returns -1 if
  // there is no such view.
  int GetIndexForPage(HorizontalPage* view) const;

  // Gets the currently selected horizontal page.
  HorizontalPage* GetSelectedPage();
  const HorizontalPage* GetSelectedPage() const;

  // Gets the offset for the horizontal page with specified index.
  gfx::Vector2d GetOffsetForPageIndex(int index) const;

  // Manages the pagination for the horizontal pages.
  ash::PaginationModel pagination_model_{this};

  // Must appear after |pagination_model_|.
  std::unique_ptr<ash::PaginationController> pagination_controller_;

  ContentsView* contents_view_;  // Not owned

  // Owned by view hierarchy:
  AppsContainerView* apps_container_view_ = nullptr;

  // The child page views. Owned by the views hierarchy.
  std::vector<HorizontalPage*> horizontal_pages_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalPageContainer);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_HORIZONTAL_PAGE_CONTAINER_H_
