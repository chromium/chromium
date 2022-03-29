// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_fetcher.h"

#include "base/values.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_features.h"
#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_client.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {
namespace {

constexpr char kIndexStoragePath[] = "launcher/remote_url_index.json";

}

RemoteUrlFetcher::RemoteUrlFetcher(Profile* profile) {
  GURL url;

  // TODO(crbug.com/1244221): Enabled state should also depend on whether we can
  // find a url.
  enabled_ = IsRemoteUrlSearchEnabled();
  if (enabled_) {
    index_ = std::make_unique<RemoteUrlIndex>(
        std::make_unique<RemoteUrlClient>(url),
        profile->GetPath().AppendASCII(kIndexStoragePath));
  }
}

RemoteUrlFetcher::~RemoteUrlFetcher() = default;

void RemoteUrlFetcher::GetApps(ResultCallback callback) {
  if (!IsRemoteUrlSearchEnabled()) {
    std::move(callback).Run({}, DiscoveryError::kErrorRequestFailed);
    return;
  }

  // TODO(crbug.com/1244221): Unimplemented.
  std::move(callback).Run({}, DiscoveryError::kErrorRequestFailed);
}

}  // namespace apps
