// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string_view>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/navigation_throttle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

static_assert(BUILDFLAG(IS_CHROMEOS));

namespace apps {

class PackageId;

class AppInstallNavigationThrottle : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  // Possibly creates a navigation throttle that handles special instructions to
  // install an app on Chrome OS.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  // Exposed for testing.
  static absl::optional<PackageId> ExtractPackageId(std::string_view query);

  explicit AppInstallNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  AppInstallNavigationThrottle(const AppInstallNavigationThrottle&) = delete;
  AppInstallNavigationThrottle& operator=(const AppInstallNavigationThrottle&) =
      delete;
  ~AppInstallNavigationThrottle() override;

  // content::NavigationHandle:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

 private:
  ThrottleCheckResult HandleRequest();
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_NAVIGATION_THROTTLE_H_
