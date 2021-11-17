// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_ANCHORED_DIALOG_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_ANCHORED_DIALOG_H_

#include <memory>

#include "base/callback.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
}

namespace views {
class DialogDelegateView;
}

namespace ash {

// A helper to keep track and manage bounds of a dialog window anchored within
// the search results app list page.
class SearchResultPageAnchoredDialog : public views::WidgetObserver {
 public:
  // Creates a widget for the provided dialog delegate view.
  // |dialog| - The dialog delegate view whose widget this should manage.
  // |host_view| - The view that is "hosting" the dialog - the dialog widget
  //     will be parented by the host view's widget, and anchor bounds provided
  //     to UpdateBounds() will be expected to be in host view bounds.
  // |callback| - A closure called when the dialog widget gets closed. The
  //     callback will not be called when the SearchResultPageAnchoredDialog
  //     gets destroyed,
  SearchResultPageAnchoredDialog(
      std::unique_ptr<views::DialogDelegateView> dialog,
      views::View* host_view,
      base::OnceClosure callback);
  SearchResultPageAnchoredDialog(const SearchResultPageAnchoredDialog& other) =
      delete;
  SearchResultPageAnchoredDialog& operator=(
      const SearchResultPageAnchoredDialog& other) = delete;
  ~SearchResultPageAnchoredDialog() override;

  // Repositions the dialog widget bounds relative to the provided anchor
  // bounds.
  // |anchor_bounds| are expected to be in |host_view_| coordinates.
  void UpdateBounds(const gfx::Rect& anchor_bounds);

  // Adjusts the vertical translate offset to be used during search results page
  // animation. The default offset follows the search box bounds translation
  // within the host view bounds.
  float AdjustVerticalTransformOffset(float default_offset);

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  views::Widget* widget() { return widget_; }

 private:
  views::Widget* widget_ = nullptr;

  views::View* const host_view_;

  base::OnceClosure callback_;

  // Most recent anchor bounds (in |host_view_| coordinates).
  gfx::Rect anchor_bounds_;

  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_PAGE_ANCHORED_DIALOG_H_
