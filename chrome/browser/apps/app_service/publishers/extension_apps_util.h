// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_UTIL_H_

#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/browser/uninstall_reason.h"

namespace apps {

// Converts an apps UninstallSource to an extension uninstall reason.
extensions::UninstallReason GetExtensionUninstallReason(
    UninstallSource uninstall_source);
}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_UTIL_H_
