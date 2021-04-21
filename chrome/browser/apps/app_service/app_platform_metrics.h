// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_

#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace apps {

class AppUpdate;

// Records metrics when launching apps.
void RecordAppLaunchMetrics(Profile* profile,
                            const apps::AppUpdate& update,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::LaunchContainer container);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_H_
