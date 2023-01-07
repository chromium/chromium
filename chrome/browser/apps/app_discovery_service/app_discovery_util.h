// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/apps/app_discovery_service/result.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace apps {

enum class ResultType {
  kTestType,
  kRecommendedArcApps,
  kGameSearchCatalog,
};

enum class AppSource {
  kTestSource,
  kPlay,
  kGames,
};

// These values persist to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class DiscoveryError {
  kSuccess,             // Successfully got app data to return.
  kErrorRequestFailed,  // Failed to get requested data.
  kErrorMalformedData,  // Failed to parse received data.
  kMaxValue = kErrorMalformedData,
};

using ResultCallback =
    base::OnceCallback<void(const std::vector<Result>& results,
                            DiscoveryError error)>;

using RepeatingResultCallback =
    base::RepeatingCallback<void(const std::vector<Result>& results)>;

using ResultCallbackList =
    base::RepeatingCallbackList<void(const std::vector<Result>& results)>;

using GetIconCallback =
    base::OnceCallback<void(const gfx::ImageSkia& image, DiscoveryError error)>;

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_UTIL_H_
