// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UTILS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UTILS_H_

#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"

namespace apps {

// Check the promise status to confirm whether the app installation has
// completed (successfully or unsuccessfully).
bool IsPromiseAppCompleted(apps::PromiseStatus status);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PROMISE_APPS_PROMISE_APP_UTILS_H_
