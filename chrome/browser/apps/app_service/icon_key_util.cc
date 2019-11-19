// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/icon_key_util.h"

namespace apps_util {

IncrementingIconKeyFactory::IncrementingIconKeyFactory() : last_timeline_(0) {}

apps::mojom::IconKeyPtr IncrementingIconKeyFactory::MakeIconKey(
    uint32_t icon_effects) {
  return apps::mojom::IconKey::New(
      ++last_timeline_, apps::mojom::IconKey::kInvalidResourceId, icon_effects);
}

}  // namespace apps_util
