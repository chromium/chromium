// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENTERPRISE_INCOGNITO_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_ENTERPRISE_INCOGNITO_NAVIGATION_THROTTLE_H_

#include <string>
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/navigation_throttle.h"

class Profile;

namespace content {
class NavigationHandle;
}  // namespace content

namespace chromeos {

// A `content::NavigationThrottle` implementation that blocks navigation in
// Incognito mode unless the user enables a list of mandatory extensions to run
// in Incognito mode. The list of mandatory extensions is configured by the
// administrator via the `MandatoryExtensionsForIncognitoNavigation` policy.
class IncognitoNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<IncognitoNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  explicit IncognitoNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      Profile* profile);
  IncognitoNavigationThrottle(const IncognitoNavigationThrottle&) = delete;
  IncognitoNavigationThrottle& operator=(const IncognitoNavigationThrottle&) =
      delete;
  ~IncognitoNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  // Verifies if extensions specified by the
  // `MandatoryExtensionsForIncognitoNavigation` policy are installed and
  // allowed to run in Incognito and populates the `blocking_extensions_` and
  // `missing_extensions_` lists accordingly.
  void ReadMandatoryExtensionsStatus();

  // Name of extensions included in the
  // `MandatoryExtensionsForIncognitoNavigation` policy which are not allowed by
  // the user to run in Incognito mode.
  base::Value::List blocking_extensions_;

  // IDs of extensions included in the
  // `MandatoryExtensionsForIncognitoNavigation` policy which are not installed
  // in the browser.
  base::Value::List missing_extensions_;

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<IncognitoNavigationThrottle> weak_ptr_factory_{this};
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ENTERPRISE_INCOGNITO_NAVIGATION_THROTTLE_H_
