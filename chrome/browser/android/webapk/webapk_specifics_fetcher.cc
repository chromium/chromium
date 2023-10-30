// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_specifics_fetcher.h"

#include "chrome/browser/android/webapps/webapp_registry.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

namespace webapk {

WebApkSpecificsFetcher::WebApkSpecificsFetcher() = default;
WebApkSpecificsFetcher::~WebApkSpecificsFetcher() = default;

std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>
WebApkSpecificsFetcher::GetWebApkSpecifics() const {
  return webapp_registry_.GetWebApkSpecifics();
}

}  // namespace webapk
