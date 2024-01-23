// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFOS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFOS_H_

#include <utility>
#include <vector>

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_info.h"

std::vector<std::pair<CrosAppsApiId, CrosAppsApiInfo>>
CreateDefaultCrosAppsApiInfo();

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_INFOS_H_
