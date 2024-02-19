// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_VIEW_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/picker/metrics/picker_session_metrics.h"
#include "ash/picker/model/picker_category.h"
#include "ash/public/cpp/ash_web_view.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Widget;
class NonClientFrameView;
}  // namespace views

namespace ash {

class BubbleEventFilter;
class PickerContentsView;
class PickerSearchFieldView;
class PickerSearchResult;
class PickerSearchResults;
class PickerSearchResultsView;
class PickerViewDelegate;
class PickerZeroStateView;
class PickerCategoryView;
class SystemShadow;

// View for the Picker widget.
class ASH_EXPORT PickerView : public views::WidgetDelegateView {
  METADATA_HEADER(PickerView, views::WidgetDelegateView)

 public:
  enum class PickerLayoutType {
    kResultsBelowSearchField,
    kResultsAboveSearchField,
  };

  // `delegate` must remain valid for the lifetime of this class.
  explicit PickerView(PickerViewDelegate* delegate,
                      base::TimeTicks trigger_event_timestamp,
                      PickerLayoutType layout_type);
  PickerView(const PickerView&) = delete;
  PickerView& operator=(const PickerView&) = delete;
  ~PickerView() override;

  // `trigger_event_timestamp` is the timestamp of the event that triggered the
  // Widget to be created. For example, if the feature was triggered by a mouse
  // click, then it should be the timestamp of the click. By default, the
  // timestamp is the time this function is called.
  // `delegate` must remain valid for the lifetime of the created Widget.
  // `caret_bounds` and `cursor_point` should be in screen coordinates.
  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& caret_bounds,
      const gfx::Point& cursor_point,
      const gfx::Rect& focused_window_bounds,
      PickerViewDelegate* delegate,
      base::TimeTicks trigger_event_timestamp = base::TimeTicks::Now());

  // views::WidgetDelegateView:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // Returns the target bounds for this Picker view. The target bounds try to
  // vertically align `search_field_view_` with `anchor_bounds`. `anchor_bounds`
  // and returned bounds should be in screen coordinates.
  gfx::Rect GetTargetBounds(const gfx::Rect& anchor_bounds,
                            PickerLayoutType layout_type);

  PickerSearchFieldView& search_field_view_for_testing() {
    return *search_field_view_;
  }
  PickerContentsView& contents_view_for_testing() { return *contents_view_; }
  PickerSearchResultsView& search_results_view_for_testing() {
    return *search_results_view_;
  }
  PickerCategoryView& category_view_for_testing() { return *category_view_; }
  PickerZeroStateView& zero_state_view_for_testing() {
    return *zero_state_view_;
  }

 private:
  // Starts a search with `query`, with search results being returned to
  // `PublishSearchResults`.
  void StartSearch(const std::u16string& query);

  // Displays `results` in the search view.
  void PublishSearchResults(const PickerSearchResults& results);

  // Selects a search result.
  void SelectSearchResult(const PickerSearchResult& result);

  // Selects a category. This shows the category view and fetches results for
  // the category, which are returned to `PublishCategoryResults`.
  void SelectCategory(PickerCategory category);

  // Displays `results` in the category view.
  void PublishCategoryResults(const PickerSearchResults& results);

  void OnClickOutsideWidget();

  void AddSearchFieldView();
  void AddContentsView(PickerLayoutType layout_type);

  std::optional<PickerCategory> selected_category_;

  // Used to close the Picker widget when the user clicks outside of it.
  std::unique_ptr<BubbleEventFilter> bubble_event_filter_;

  std::unique_ptr<SystemShadow> shadow_;

  PickerSessionMetrics session_metrics_;
  raw_ptr<PickerViewDelegate> delegate_ = nullptr;

  raw_ptr<PickerSearchFieldView> search_field_view_ = nullptr;
  raw_ptr<PickerContentsView> contents_view_ = nullptr;
  raw_ptr<PickerZeroStateView> zero_state_view_ = nullptr;
  raw_ptr<PickerCategoryView> category_view_ = nullptr;
  raw_ptr<PickerSearchResultsView> search_results_view_ = nullptr;

  // Whether the first set of results for the current search have been published
  // yet.
  bool published_first_results_ = false;

  base::WeakPtrFactory<PickerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_H_
