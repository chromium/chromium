// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Widget;
class NonClientFrameView;
}  // namespace views

namespace ash {

// View for the Picker widget.
class PickerView : public views::WidgetDelegateView {
 public:
  METADATA_HEADER(PickerView);

  PickerView();
  PickerView(const PickerView&) = delete;
  PickerView& operator=(const PickerView&) = delete;
  ~PickerView() override;

  static views::UniqueWidgetPtr CreateWidget();

  // views::WidgetDelegateView:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_VIEW_H_
