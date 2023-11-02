// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"

namespace apps_util {

IncrementingIconKeyFactory::IncrementingIconKeyFactory() = default;

apps::mojom::IconKeyPtr IncrementingIconKeyFactory::MakeIconKey(
    uint32_t icon_effects) {
  return apps::mojom::IconKey::New(
      ++last_timeline_, apps::mojom::IconKey::kInvalidResourceId, icon_effects);
}

std::unique_ptr<apps::IconKey> IncrementingIconKeyFactory::CreateIconKey(
    uint32_t icon_effects) {
  return std::make_unique<apps::IconKey>(
      ++last_timeline_, apps::IconKey::kInvalidResourceId, icon_effects);
}

}  // namespace apps_util
