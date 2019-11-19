// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"

namespace ash {

// static
ShelfItem ShelfTestUtil::AddAppShortcut(const std::string id,
                                        const ShelfItemType type) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  ShelfItem item;
  item.type = type;
  if (type == TYPE_APP)
    item.status = STATUS_RUNNING;
  item.id = ShelfID(id);
  controller->model()->Add(item);
  return item;
}

}  // namespace ash
