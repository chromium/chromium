// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CONTROLLER_H_
#define ASH_PICKER_PICKER_CONTROLLER_H_

#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// Controls a Picker widget.
class PickerController {
 public:
  // Whether the provided feature key for Picker can enable the feature.
  static bool IsFeatureKeyMatched();

  // Toggles the visibility of the Picker widget.
  void ToggleWidget();

  // Returns the Picker widget for tests.
  views::Widget* widget_for_testing() { return widget_.get(); }

 private:
  views::UniqueWidgetPtr widget_;
};

}  // namespace ash

#endif
