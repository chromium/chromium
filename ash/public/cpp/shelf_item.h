// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_ITEM_H_
#define ASH_PUBLIC_CPP_SHELF_ITEM_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ShelfItem {
  ShelfItem();
  ShelfItem(const ShelfItem& shelf_item);
  ~ShelfItem();

  // Returns true if the pin state of the item is forced and can not be changed.
  bool IsPinStateForced() const;

  ShelfItemType type = TYPE_UNDEFINED;

  // Image to display in the shelf.
  gfx::ImageSkia image;

  // Running status.
  ShelfItemStatus status = STATUS_CLOSED;

  // The application id and launch id for this shelf item.
  ShelfID id;

  // The title to display for tooltips, etc.
  std::u16string title;

  SkColor notification_badge_color = SK_ColorWHITE;

  // App status.
  AppStatus app_status = AppStatus::kReady;

  // Whether the item is associated with a window in the currently active desk.
  // This value is valid only when |features::kPerDeskShelf| is enabled.
  // Otherwise it won't be updated and will always be true.
  bool is_on_active_desk = true;

  // Whether the tooltip should be shown on hover; generally true.
  bool shows_tooltip = true;

  // Whether the item is pinned by a policy preference. If so, pin state should
  // not be modifiable by user.
  bool pinned_by_policy = false;

  // Whether the item pin state is forced according to its app type. The pin
  // state can not be modified by user if this is set to true.
  bool pin_state_forced_by_type = false;

  // Whether the item has a notification.
  bool has_notification = false;
};

typedef std::vector<ShelfItem> ShelfItems;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_ITEM_H_
