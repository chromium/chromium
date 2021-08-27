// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/apps_fetcher_service/recommended_arc_apps_fetcher.h"

#include "base/values.h"

namespace apps {

void RecommendedArcAppsFetcher::GetApps(ResultCallback callback) {
  // TODO(crbug.com/1223321) : Implement.
  std::move(callback).Run(base::Value());
}

}  // namespace apps
