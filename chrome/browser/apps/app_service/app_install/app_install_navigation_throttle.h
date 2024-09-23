// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_INSTALL_APP_INSTALL_NAVIGATION_THROTTLE_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "content/public/browser/navigation_throttle.h"

static_assert(BUILDFLAG(IS_CHROMEOS));

namespace apps {

class PackageId;

// Matches URIs of the form almanac://install-app?package_id=<package id> and
// triggers an installation using app metadata from Almanac.
// Design doc: go/app-install-service-uri
class AppInstallNavigationThrottle : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  static base::OnceCallback<void(bool created)>&
  MaybeCreateCallbackForTesting();

  // Possibly creates a navigation throttle that handles special instructions to
  // install an app on Chrome OS.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  // Exposed for testing.
  struct QueryParams {
    QueryParams();
    QueryParams(std::optional<std::string> serialized_package_id,
                AppInstallSurface source);
    QueryParams(QueryParams&&);
    ~QueryParams();
    bool operator==(const QueryParams& other) const;

    // This is a std::string instead of a PackageId because Chrome should still
    // attempt to handle PackageIds it does not yet understand and fallback to
    // install URL behavior.
    std::optional<std::string> serialized_package_id;
    AppInstallSurface source = AppInstallSurface::kAppInstallUriUnknown;
  };
  static QueryParams ExtractQueryParams(std::string_view query);

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
