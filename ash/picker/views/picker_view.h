// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/picker/picker_session_metrics.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Widget;
class NonClientFrameView;
}  // namespace views

namespace ash {

class PickerSearchFieldView;
class PickerUserEducationView;
class PickerZeroStateView;
class PickerContentsView;
class PickerSearchResults;

// View for the Picker widget.
class ASH_EXPORT PickerView : public views::WidgetDelegateView {
 public:
  METADATA_HEADER(PickerView);

  class Delegate {
   public:
    using SearchResultsCallback =
        base::RepeatingCallback<void(const PickerSearchResults& results)>;

    virtual ~Delegate() {}
    virtual std::unique_ptr<AshWebView> CreateWebView(
        const AshWebView::InitParams& params) = 0;

    // Starts a search for `query`. Results will be returned via `callback`,
    // which may be called multiples times to update the results.
    virtual void StartSearch(const std::u16string& query,
                             SearchResultsCallback callback) = 0;
  };

  explicit PickerView(std::unique_ptr<Delegate> delegate,
                      base::TimeTicks trigger_event_timestamp);
  PickerView(const PickerView&) = delete;
  PickerView& operator=(const PickerView&) = delete;
  ~PickerView() override;

  // `trigger_event_timestamp` is the timestamp of the event that triggered the
  // Widget to be created. For example, if the feature was triggered by a mouse
  // click, then it should be the timestamp of the click. By default, the
  // timestamp is the time this function is called.
  static views::UniqueWidgetPtr CreateWidget(
      std::unique_ptr<Delegate> delegate,
      base::TimeTicks trigger_event_timestamp = base::TimeTicks::Now());

  // views::WidgetDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  PickerSearchFieldView& search_field_view_for_testing() {
    return *search_field_view_;
  }
  PickerZeroStateView& zero_state_view_for_testing() {
    return *zero_state_view_;
  }
  views::View& search_results_view_for_testing() {
    return *search_results_view_;
  }

 private:
  // Starts a search with `query`, with search results being returned to
  // `PublishSearchResults`.
  void StartSearch(const std::u16string& query);

  // Displays `results` in the view.
  void PublishSearchResults(const PickerSearchResults& results);

  PickerSessionMetrics session_metrics_;
  std::unique_ptr<Delegate> delegate_;
  raw_ptr<PickerSearchFieldView> search_field_view_ = nullptr;
  raw_ptr<PickerContentsView> contents_view_ = nullptr;
  raw_ptr<PickerZeroStateView> zero_state_view_ = nullptr;
  raw_ptr<views::View> search_results_view_ = nullptr;
  raw_ptr<PickerUserEducationView> user_education_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_H_
