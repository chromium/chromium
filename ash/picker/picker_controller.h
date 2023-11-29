// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_CONTROLLER_H_
#define ASH_PICKER_PICKER_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class PickerClient;

// Controls a Picker widget.
class ASH_EXPORT PickerController {
 public:
  PickerController();
  PickerController(const PickerController&) = delete;
  PickerController& operator=(const PickerController&) = delete;
  ~PickerController();

  // Whether the provided feature key for Picker can enable the feature.
  static bool IsFeatureKeyMatched();

  // Sets the `client` used by this class and the widget to communicate with the
  // browser. `client` may be set to null, which will close the Widget if it's
  // open. If `client` is not null, then it must remain valid for the lifetime
  // of this class, or until `SetClient` is called with a different client.
  void SetClient(PickerClient* client);

  // Toggles the visibility of the Picker widget.
  // This must only be called after `SetClient` is called with a valid client.
  void ToggleWidget();

  // Returns the Picker widget for tests.
  views::Widget* widget_for_testing() { return widget_.get(); }

 private:
  raw_ptr<PickerClient> client_ = nullptr;
  views::UniqueWidgetPtr widget_;
};

}  // namespace ash

#endif
