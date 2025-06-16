// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_THROTTLE_H_

#include "base/memory/raw_ptr.h"
#include "components/collaboration/public/collaboration_service.h"
#include "content/public/browser/navigation_throttle.h"

namespace data_sharing {

// This class allows blocking a navigation for data sharing related URLs.
class DataSharingNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(
      content::NavigationThrottleRegistry& registry);
  explicit DataSharingNavigationThrottle(
      content::NavigationThrottleRegistry& registry);

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

  // Set a CollaborationService to use for testing.
  void SetServiceForTesting(collaboration::CollaborationService* test_service);

 private:
  ThrottleCheckResult CheckIfShouldIntercept();

  raw_ptr<collaboration::CollaborationService> test_service_;
};

}  // namespace data_sharing

#endif  // CHROME_BROWSER_DATA_SHARING_DATA_SHARING_NAVIGATION_THROTTLE_H_
