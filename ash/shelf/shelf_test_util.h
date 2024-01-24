// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TEST_UTIL_H_
#define ASH_SHELF_SHELF_TEST_UTIL_H_

#include <string>

#include "ash/public/cpp/shelf_item.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class ShelfTestUtil {
 public:
  ShelfTestUtil(const ShelfTestUtil&) = delete;
  ShelfTestUtil& operator=(const ShelfTestUtil&) = delete;

  // Adds an application shortcut to the shelf model, with the given identifier
  // and the given shelf item type.
  static ShelfItem AddAppShortcut(const std::string& id, ShelfItemType type);

  // Adds an application shortcut to the shelf model, with the given identifier,
  // the given shelf item type, and the given icon.
  static ShelfItem AddAppShortcutWithIcon(const std::string& id,
                                          ShelfItemType type,
                                          gfx::ImageSkia icon);

  // Adds an web app shortcut to the shelf model, with the given identifier, the
  // given shelf item type, the given icon, and the given badge icon.
  static ShelfItem AddWebAppShortcut(const std::string& id,
                                     bool pinned,
                                     gfx::ImageSkia icon,
                                     gfx::ImageSkia badge_icon);

  // Adds an app that is not pinnable to the shelf model.
  static ShelfItem AddAppNotPinnable(const std::string& id);
};

// Waits for an overview enter animation if |enter|; waits for an overview exit
// animation otherwise.
void WaitForOverviewAnimation(bool enter);

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TEST_UTIL_H_
