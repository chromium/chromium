// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/draggable_test_view.h"

#include "base/files/file_path.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"

namespace ash {

DraggableTestView::DraggableTestView(bool set_drag_image, bool set_file_name)
    : set_drag_image_(set_drag_image), set_file_name_(set_file_name) {
  // The drag operation is `DRAG_COPY` by default. `DraggableTestView` users
  // can use `WillOnce()` or `WillRepeatedly()` to customize the return.
  ON_CALL(*this, GetDragOperations)
      .WillByDefault(testing::Return(ui::DragDropTypes::DRAG_COPY));
}

DraggableTestView::~DraggableTestView() = default;

void DraggableTestView::WriteDragData(const gfx::Point& press_pt,
                                      ui::OSExchangeData* data) {
  data->SetString(u"test");

  if (set_file_name_) {
    data->SetFilename(base::FilePath("dummy_path"));
  }

  if (set_drag_image_) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(/*width=*/10, /*height=*/10);
    data->provider().SetDragImage(gfx::ImageSkia::CreateFrom1xBitmap(bitmap),
                                  gfx::Vector2d());
  }
}

}  // namespace ash
