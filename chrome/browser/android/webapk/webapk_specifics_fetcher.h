// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SPECIFICS_FETCHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SPECIFICS_FETCHER_H_

#include "chrome/browser/android/webapps/webapp_registry.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

namespace webapk {

// An abstract specifics fetcher mockable for testing.
class AbstractWebApkSpecificsFetcher {
 public:
  virtual ~AbstractWebApkSpecificsFetcher() = default;
  virtual std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>
  GetWebApkSpecifics() const = 0;
};

class WebApkSpecificsFetcher : public AbstractWebApkSpecificsFetcher {
 public:
  WebApkSpecificsFetcher();
  WebApkSpecificsFetcher(const WebApkSpecificsFetcher&) = delete;
  WebApkSpecificsFetcher& operator=(const WebApkSpecificsFetcher&) = delete;
  ~WebApkSpecificsFetcher() override;

  // AbstractWebApkSpecificsFetcher implementation.
  std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>> GetWebApkSpecifics()
      const override;

 private:
  // TODO(crbug.com/40287112): WebappRegistry is supposed to be owned by
  // ChromeBrowsingDataRemoverDelegate.
  WebappRegistry webapp_registry_;
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SPECIFICS_FETCHER_H_
