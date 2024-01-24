// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_test_util.h"

#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shell.h"

namespace ash {

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

  // All focusable objects are expected to have an accessible name to pass
  // the accessibility paint checks. ShelfAppButton will use the item's
  // title as the accessible name. Since this item is purely for testing,
  // use its id as the title in order for the unit tests to pass the checks.
  item.title = base::UTF8ToUTF16(id);
  controller->model()->Add(item,
                           std::make_unique<TestShelfItemDelegate>(item.id));
  return item;
}

ShelfItem ShelfTestUtil::AddWebAppShortcut(const std::string& id,
                                           bool pinned,
                                           gfx::ImageSkia icon,
                                           gfx::ImageSkia badge_icon) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  ShelfItem item;
  pinned ? item.type = TYPE_PINNED_APP : TYPE_APP;
  if (item.type == TYPE_APP) {
    item.status = STATUS_RUNNING;
  }
  item.id = ShelfID(id);
  item.image = icon;
  item.badge_image = badge_icon;

  // All focusable objects are expected to have an accessible name to pass
  // the accessibility paint checks. ShelfAppButton will use the item's
  // title as the accessible name. Since this item is purely for testing,
  // use its id as the title in order for the unit tests to pass the checks.
  item.title = base::UTF8ToUTF16(id);
  controller->model()->Add(item,
                           std::make_unique<TestShelfItemDelegate>(item.id));
  return item;
}

// static
ShelfItem ShelfTestUtil::AddAppNotPinnable(const std::string& id) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  ShelfItem item;
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  item.id = ShelfID(id);
  item.pin_state_forced_by_type = true;

  // All focusable objects are expected to have an accessible name to pass
  // the accessibility paint checks. ShelfAppButton will use the item's
  // title as the accessible name. Since this item is purely for testing,
  // use its id as the title in order for the unit tests to pass the checks.
  item.title = base::UTF8ToUTF16(id);
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
