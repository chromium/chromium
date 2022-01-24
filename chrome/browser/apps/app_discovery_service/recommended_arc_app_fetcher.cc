// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/recommended_arc_app_fetcher.h"

#include <utility>

namespace apps {

void RecommendedArcAppFetcher::GetApps(ResultCallback callback) {
  // TODO(crbug.com/1223321) : Implement.
  std::move(callback).Run({});
}

}  // namespace apps
