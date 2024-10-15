// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/coral_util.h"

namespace ash::coral_util {

std::string ASH_PUBLIC_EXPORT
GetIdentifier(const coral::mojom::EntityPtr& item) {
  if (item->is_app()) {
    return item->get_app()->id;
  }
  if (item->is_tab()) {
    return item->get_tab()->url.possibly_invalid_spec();
  }
  NOTREACHED();
}

std::string ASH_PUBLIC_EXPORT GetIdentifier(const coral::mojom::Entity& item) {
  if (item.is_app()) {
    return item.get_app()->id;
  }
  if (item.is_tab()) {
    return item.get_tab()->url.possibly_invalid_spec();
  }
  NOTREACHED();
}

}  // namespace ash::coral_util
