// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAGGABLE_TEST_VIEW_H_
#define ASH_DRAG_DROP_DRAGGABLE_TEST_VIEW_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/view.h"

namespace ash {

// A simple drggable view for test.
class DraggableTestView : public views::View {
 public:
  explicit DraggableTestView(bool set_drag_image = false,
                             bool set_file_name = false);
  DraggableTestView(const DraggableTestView&) = delete;
  DraggableTestView& operator=(const DraggableTestView&) = delete;
  ~DraggableTestView() override;

  // views::View overrides:
  MOCK_METHOD(int, GetDragOperations, (const gfx::Point& press_pt), (override));
  void WriteDragData(const gfx::Point& press_pt, OSExchangeData* data) override;

 private:
  // True if drag data needs a drag image.
  // NOTE: gesture drag needs a drag image.
  const bool set_drag_image_;

  // True if drag data needs a file path.
  const bool set_file_name_;
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAGGABLE_TEST_VIEW_H_
