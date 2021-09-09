// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_fetcher.h"

#include "base/values.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_features.h"

namespace apps {

RemoteUrlFetcher::RemoteUrlFetcher(Profile* profile) {}

void RemoteUrlFetcher::GetApps(ResultCallback callback) {
  if (!IsRemoteUrlSearchEnabled()) {
    std::move(callback).Run({});
    return;
  }

  // TODO(crbug.com/1244221): Unimplemented.
  std::move(callback).Run({});
}

}  // namespace apps
