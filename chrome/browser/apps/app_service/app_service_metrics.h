// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_

#include <map>
#include <string>

#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

class AppUpdate;

// The built-in app's histogram name. This is used for logging so do not change
// the order of this enum.
enum class BuiltInAppName {
  kKeyboardShortcutViewer = 0,
  kSettings = 1,
  kContinueReading = 2,
  kCameraDeprecated = 3,
  // kDiscover = 4, obsolete
  kPluginVm = 5,
  kReleaseNotes = 6,
  kMaxValue = kReleaseNotes,
};

void RecordAppLaunch(const std::string& app_id,
                     apps::mojom::LaunchSource launch_source);

void RecordBuiltInAppSearchResult(const std::string& app_id);

void RecordAppBounce(const apps::AppUpdate& app);

void RecordAppsPerNotification(int count);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_METRICS_H_
