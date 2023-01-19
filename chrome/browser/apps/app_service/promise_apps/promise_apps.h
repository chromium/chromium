// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APPS_H_

#include "chrome/browser/apps/app_service/package_id.h"

namespace apps {

// A promise app is a barebones app object created to show an app's icon and
// name in the Launcher/Shelf while the package is currently installing
// or pending installation. Each pending package installation is represented by
// its own promise app.
struct PromiseApp {
 public:
  explicit PromiseApp(const apps::PackageId& package_id)
      : package_id(package_id) {}
  PackageId package_id;
  float progress;
};

using PromiseAppPtr = std::unique_ptr<PromiseApp>;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APPS_H_
