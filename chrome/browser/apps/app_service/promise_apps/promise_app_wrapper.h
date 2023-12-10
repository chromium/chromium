// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_WRAPPER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_WRAPPER_H_

#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "url/gurl.h"

namespace apps {

class IconWrapper {
 public:
  explicit IconWrapper(proto::PromiseAppResponse::Icon icon_proto);
  GURL GetUrl() const;
  std::optional<int> GetWidthInPixels() const;
  std::string GetMimeType() const;
  bool IsMaskingAllowed() const;

 private:
  proto::PromiseAppResponse::Icon icon_proto_;
};

// A wrapper class around a Promise App proto to allow for easier
// extraction and conversion of information.
class PromiseAppWrapper {
 public:
  explicit PromiseAppWrapper(proto::PromiseAppResponse promise_app_proto);
  PromiseAppWrapper(const PromiseAppWrapper&);
  PromiseAppWrapper& operator=(const PromiseAppWrapper&);
  ~PromiseAppWrapper();

  std::optional<PackageId> GetPackageId() const;
  std::optional<std::string> GetName() const;
  std::vector<IconWrapper> GetIcons() const;

 private:
  proto::PromiseAppResponse promise_app_proto_;
  std::optional<PackageId> package_id_;
};

std::ostream& operator<<(std::ostream& os, const IconWrapper& icon);
std::ostream& operator<<(std::ostream& os,
                         const PromiseAppWrapper& promise_app);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_WRAPPER_H_
