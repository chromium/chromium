// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_PAGE_H_

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/model/app_list_model.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace ash {

class ContentsView;

class APP_LIST_EXPORT AppListPage : public views::View {
 public:
  AppListPage();
  ~AppListPage() override;

  // Triggered when the page is about to be shown.
  virtual void OnWillBeShown();

  // Triggered after the page has been shown.
  virtual void OnShown();

  // Triggered when the page is about to be hidden.
  virtual void OnWillBeHidden();

  // Triggered after the page has been hidden.
  virtual void OnHidden();

  // Triggered when the page transition animation started.
  virtual void OnAnimationStarted(ash::AppListState from_state,
                                  ash::AppListState to_state) = 0;

  // Triggered after the page transition animation has updated.
  virtual void OnAnimationUpdated(double progress,
                                  ash::AppListState from_state,
                                  ash::AppListState to_state);

  // Returns the search box size that is preferred by the page. Used by
  // ContentsView to calculate the search box widget bounds that
  // should be used on this page.
  //
  // If this method returns an empty size, the ContentsView will use
  // the default search box size.
  // Default implementation returns an empty size.
  virtual gfx::Size GetPreferredSearchBoxSize() const;

  // Returns the preferred search box origin's y coordinate within the app list
  // contents view bounds for the provided app list view state. Used by
  // ContentsView to calculate the search box widget bounds that
  // should be used on this page.
  //
  // If this returns base::nullopt, the ContentsView will use default
  // y value for the search box origin.
  // The default implementation return base::nullopt.
  //
  // NOTE: The search box will be horizontally centered in the app list contents
  // bounds, if a different behavior is required, this method should be changed
  // to return an origin point instead of just Y coordinate.
  virtual base::Optional<int> GetSearchBoxTop(
      ash::AppListViewState view_state) const;

  // Returns the intended page bounds when the app list is in the provided
  // state.
  // |contents_bounds| - The current app list contents bounds.
  // |search_box_bounds| - The expected search box bounds when the app list is
  //                       in state |state|.
  virtual gfx::Rect GetPageBoundsForState(
      ash::AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const = 0;

  // Should update the app list page opacity for the current state. Called when
  // the selected page changes without animation - if the page implements this,
  // it should make sure the page transition animation updates the opacity as
  // well.
  // Default implementation is no-op.
  virtual void UpdateOpacityForState(ash::AppListState state);

  // Convenience method that sets the page bounds to the bounds returned by
  // GetPageBoundsForState().
  void UpdatePageBoundsForState(ash::AppListState state,
                                const gfx::Rect& contents_bounds,
                                const gfx::Rect& search_box_bounds);

  const ContentsView* contents_view() const { return contents_view_; }
  void set_contents_view(ContentsView* contents_view) {
    contents_view_ = contents_view;
  }

  // Returns selected view in this page.
  virtual views::View* GetSelectedView() const;

  // Returns the first focusable view in this page.
  virtual views::View* GetFirstFocusableView();

  // Returns the last focusable view in this page.
  virtual views::View* GetLastFocusableView();

  // Returns true if the search box should be shown in this page.
  virtual bool ShouldShowSearchBox() const;

  // Returns the area above the contents view, given the desired size of this
  // page, in the contents view's coordinate space.
  gfx::Rect GetAboveContentsOffscreenBounds(const gfx::Size& size) const;

  // Returns the area below the contents view, given the desired size of this
  // page, in the contents view's coordinate space.
  gfx::Rect GetBelowContentsOffscreenBounds(const gfx::Size& size) const;

  // Returns the entire bounds of the contents view, in the contents view's
  // coordinate space.
  gfx::Rect GetFullContentsBounds() const;

  // Returns the default bounds of pages inside the contents view, in the
  // contents view's coordinate space. This is the area of the contents view
  // below the search box.
  gfx::Rect GetDefaultContentsBounds() const;

  // views::View:
  const char* GetClassName() const override;

 private:
  ContentsView* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(AppListPage);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_PAGE_H_
