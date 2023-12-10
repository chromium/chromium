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

// View for the Picker widget.
class ASH_EXPORT PickerView : public views::WidgetDelegateView {
 public:
  METADATA_HEADER(PickerView);

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual std::unique_ptr<AshWebView> CreateWebView(
        const AshWebView::InitParams& params) = 0;
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

  const AshWebView& web_view_for_testing() const { return *web_view_; }

 private:
  PickerSessionMetrics session_metrics_;
  raw_ptr<PickerSearchFieldView> search_field_view_ = nullptr;
  raw_ptr<AshWebView> web_view_ = nullptr;
  raw_ptr<PickerUserEducationView> user_education_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_H_
