// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/coral_util.h"

namespace ash::coral_util {

std::string ASH_PUBLIC_EXPORT
GetIdentifier(const coral::mojom::EntityKeyPtr& key) {
  if (key->is_app_id()) {
    return key->get_app_id();
  }
  if (key->is_tab_url()) {
    return key->get_tab_url().possibly_invalid_spec();
  }
  NOTREACHED();
}

std::string ASH_PUBLIC_EXPORT
GetIdentifier(const coral::mojom::EntityKey& key) {
  if (key.is_app_id()) {
    return key.get_app_id();
  }
  if (key.is_tab_url()) {
    return key.get_tab_url().possibly_invalid_spec();
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
