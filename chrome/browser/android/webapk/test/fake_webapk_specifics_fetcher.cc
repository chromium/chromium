// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/test/fake_webapk_specifics_fetcher.h"

#include "components/sync/protocol/web_apk_specifics.pb.h"

namespace webapk {

FakeWebApkSpecificsFetcher::FakeWebApkSpecificsFetcher()
    : specifics_(std::make_unique<
                 std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>>()) {}
FakeWebApkSpecificsFetcher::~FakeWebApkSpecificsFetcher() = default;

void FakeWebApkSpecificsFetcher::SetWebApkSpecifics(
    std::unique_ptr<std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>>
        specifics) {
  specifics_ = std::move(specifics);
}

// AbstractWebApkSpecificsFetcher implementation.
std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>
FakeWebApkSpecificsFetcher::GetWebApkSpecifics() const {
  std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>> ret;
  for (auto& specifics : *specifics_) {
    ret.push_back(std::move(specifics));
  }
  return ret;
}

}  // namespace webapk
