// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_MODEL_DELEGATE_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_MODEL_DELEGATE_H_

#include <string>

#include "components/sync/model/string_ordinal.h"

namespace ash {

// The interface used to update app list items from ash. The browser side owns
// app list item data while the ash side is the consumer of app list item data.
// Ash classes should utilize this interface to update app list items.
// TODO(https://crbug.com/1257605): refactor the code so that the browser side
// owns app list item data.
class AppListModelDelegate {
 public:
  // Requests the owner to set the item indexed by `id` with `new_position`.
  // `id` is passed by value instead of a string reference. Because if `id`
  // is a reference to string, the method user may be misled to pass the item id
  // fetched from `AppListItem` as the parameter. It is risky because `id`
  // may be invalid if `AppListItem::SetMetadata()` is triggered.
  virtual void RequestPositionUpdate(
      std::string id,
      const syncer::StringOrdinal& new_position) = 0;

 protected:
  virtual ~AppListModelDelegate() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_MODEL_DELEGATE_H_
