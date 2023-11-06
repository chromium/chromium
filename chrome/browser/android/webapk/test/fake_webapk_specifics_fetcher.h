// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_WEBAPK_SPECIFICS_FETCHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_WEBAPK_SPECIFICS_FETCHER_H_

#include <memory>

#include "chrome/browser/android/webapk/webapk_specifics_fetcher.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

namespace webapk {

// WebApkSpecificsFetcher subclass for testing.
class FakeWebApkSpecificsFetcher : public AbstractWebApkSpecificsFetcher {
 public:
  FakeWebApkSpecificsFetcher();
  FakeWebApkSpecificsFetcher(const FakeWebApkSpecificsFetcher&) = delete;
  FakeWebApkSpecificsFetcher& operator=(const FakeWebApkSpecificsFetcher&) =
      delete;
  ~FakeWebApkSpecificsFetcher() override;

  void SetWebApkSpecifics(
      std::unique_ptr<std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>>
          specifics);

  // AbstractWebApkSpecificsFetcher implementation.
  std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>> GetWebApkSpecifics()
      const override;

 private:
  std::unique_ptr<std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>>
      specifics_;
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_TEST_FAKE_WEBAPK_SPECIFICS_FETCHER_H_
