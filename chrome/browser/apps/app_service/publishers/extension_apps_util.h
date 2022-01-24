// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_UTIL_H_

#include <string>

#include "build/chromeos_buildflags.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/browser/uninstall_reason.h"

namespace apps {

// Converts an apps UninstallSource to an extension uninstall reason.
extensions::UninstallReason GetExtensionUninstallReason(
    apps::mojom::UninstallSource uninstall_source);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Some extension apps will continue to run in ash until they are either
// deprecated or migrated. This function returns whether a given app_id is on
// that keep list. This function must only be called from the UI thread.
bool ExtensionAppRunsInAsh(const std::string& app_id);
#endif

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_EXTENSION_APPS_UTIL_H_
