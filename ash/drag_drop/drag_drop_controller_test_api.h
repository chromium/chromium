// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_TEST_API_H_
#define ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class DragDropControllerTestApi {
 public:
  explicit DragDropControllerTestApi(DragDropController* controller)
      : controller_(controller) {}

  DragDropControllerTestApi(const DragDropControllerTestApi&) = delete;
  DragDropControllerTestApi& operator=(const DragDropControllerTestApi&) =
      delete;

  ~DragDropControllerTestApi() = default;

  bool enabled() const { return controller_->enabled_; }

  views::Widget* drag_image_widget() {
    return controller_->drag_image_widget_.get();
  }

 private:
  raw_ptr<DragDropController> controller_;
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_CONTROLLER_TEST_API_H_
