// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_test_util.h"

#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"

namespace ash {

namespace {
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {}
};
}  // namespace

// static
ShelfItem ShelfTestUtil::AddAppShortcut(const std::string& id,
                                        ShelfItemType type) {
  return AddAppShortcutWithIcon(id, type, gfx::ImageSkia());
}

// static
ShelfItem ShelfTestUtil::AddAppShortcutWithIcon(const std::string& id,
                                                ShelfItemType type,
                                                gfx::ImageSkia icon) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  ShelfItem item;
  item.type = type;
  if (type == TYPE_APP)
    item.status = STATUS_RUNNING;
  item.id = ShelfID(id);
  item.image = icon;
  controller->model()->Add(item,
                           std::make_unique<TestShelfItemDelegate>(item.id));
  return item;
}

void WaitForOverviewAnimation(bool enter) {
  ShellTestApi().WaitForOverviewAnimationState(
      enter ? OverviewAnimationState::kEnterAnimationComplete
            : OverviewAnimationState::kExitAnimationComplete);
}

}  // namespace ash
