// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_PAGE_H_

#include "ash/app_list/model/app_list_model.h"
#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class ContentsView;

class ASH_EXPORT AppListPage : public views::View {
 public:
  AppListPage();

  AppListPage(const AppListPage&) = delete;
  AppListPage& operator=(const AppListPage&) = delete;

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
  virtual void OnAnimationStarted(AppListState from_state,
                                  AppListState to_state) = 0;

  // Triggered after the page transition animation has updated.
  virtual void OnAnimationUpdated(double progress,
                                  AppListState from_state,
                                  AppListState to_state);

  // Returns the search box size that is preferred by the page. Used by
  // ContentsView to calculate the search box widget bounds that
  // should be used on this page.
  //
  // If this method returns an empty size, the ContentsView will use
  // the default search box size.
  // Default implementation returns an empty size.
  virtual gfx::Size GetPreferredSearchBoxSize() const;

  // Should update the app list page opacity for the current state. Called when
  // the selected page changes without animation - if the page implements this,
  // it should make sure the page transition animation updates the opacity as
  // well.
  // |state| - The current app list state.
  // |search_box_opacity| - The current search box opacity.
  // |restore_opacity| - Whether the page opacity should be restored, e.g. when
  //     the app list drag ends. Note that |search_box_opacity| will be 1.0f if
  //     |restore_opacity| is true.
  virtual void UpdatePageOpacityForState(AppListState state,
                                         float search_box_opacity,
                                         bool restore_opacity) = 0;

  // Updates the page bounds to match the provided app list state.
  // The default implementation sets the bounds returned by
  // GetPageBoundsForState().
  // The arguments match the GetPageBoundsForState() arguments.
  virtual void UpdatePageBoundsForState(AppListState state,
                                        const gfx::Rect& contents_bounds,
                                        const gfx::Rect& search_box_bounds);

  // Returns the bounds the app list page should have for the app list state.
  // |state| - The current app list state.
  // |contents_bounds| - The current app list contents bounds.
  // |search_box_bounds| - The current search box bounds.
  virtual gfx::Rect GetPageBoundsForState(
      AppListState state,
      const gfx::Rect& contents_bounds,
      const gfx::Rect& search_box_bounds) const = 0;

  const ContentsView* contents_view() const { return contents_view_; }
  ContentsView* contents_view() { return contents_view_; }
  void set_contents_view(ContentsView* contents_view) {
    contents_view_ = contents_view;
  }

  // Returns the first focusable view in this page.
  views::View* GetFirstFocusableView();

  // Returns the last focusable view in this page.
  views::View* GetLastFocusableView();

  // Called when the app list view state changes to |target_view_state| to
  // animate the app list page opacity.
  // |current_progress| - the current app list transition progress.
  // |animator| - callback that when run starts the opacity animation.
  using OpacityAnimator =
      base::RepeatingCallback<void(views::View* view, bool target_visibility)>;
  virtual void AnimateOpacity(float current_progress,
                              AppListViewState target_view_state,
                              const OpacityAnimator& animator);

  // Called when the app list view state changes to |target_view_state| to
  // animate the app list page vertical offset from the app list view top.
  // |animator| - The callback that runs the transform animation to update the
  // page's vertical position.
  // |default_offset| - the default transform offset that can be passed to
  //     |animator| to follow the search box position animation.
  using TransformAnimator =
      base::RepeatingCallback<void(float offset, ui::Layer* layer)>;
  virtual void AnimateYPosition(AppListViewState target_view_state,
                                const TransformAnimator& animator,
                                float default_offset);

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
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_PAGE_H_
