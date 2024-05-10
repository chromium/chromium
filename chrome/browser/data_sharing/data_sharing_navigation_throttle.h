// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_THROTTLE_H_

#include "components/data_sharing/public/data_sharing_service.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}

namespace data_sharing {

// This class allows blocking a navigation for data sharing related URLs.
class DataSharingNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);
  explicit DataSharingNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

  // Set a DataSharingService to use for testing.
  void SetServiceForTesting(DataSharingService* test_service);

 private:
  ThrottleCheckResult CheckIfShouldIntercept();

  raw_ptr<DataSharingService> test_service_;
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_THROTTLE_H_
