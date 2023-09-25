// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_WEB_APPS_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_WEB_APPS_UTILS_H_

#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"

namespace apps {

struct App;

void MaybeSimulatePromiseAppInstallationEvents(apps::AppServiceProxy* proxy,
                                               apps::App* app);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_WEB_APPS_UTILS_H_
