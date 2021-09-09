// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/apps/app_discovery_service/result.h"

namespace apps {

enum class ResultType {
  kRecommendedArcApps,
  kRemoteUrlSearch,
};

enum class AppSource {
  kPlay,
};

// TODO(crbug.com/1243546): Define error management. The ResultCallback
// should have an error response to inform the consumer whether the request
// was successful.
using ResultCallback = base::OnceCallback<void(std::vector<Result> results)>;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_
