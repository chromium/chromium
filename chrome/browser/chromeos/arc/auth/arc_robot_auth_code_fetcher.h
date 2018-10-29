// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ROBOT_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ROBOT_AUTH_CODE_FETCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_code_fetcher.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace enterprise_management {
class DeviceManagementResponse;
}

namespace policy {
class DeviceManagementRequestJob;
}

namespace arc {

// This class is responsible to fetch auth code for robot account. Robot auth
// code is used for creation an account on Android side in ARC kiosk mode.
class ArcRobotAuthCodeFetcher : public ArcAuthCodeFetcher {
 public:
  ArcRobotAuthCodeFetcher();
  ~ArcRobotAuthCodeFetcher() override;

  // ArcAuthCodeFetcher:
  void Fetch(FetchCallback callback) override;

 private:
  void OnFetchRobotAuthCodeCompleted(
      FetchCallback callback,
      policy::DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  std::unique_ptr<policy::DeviceManagementRequestJob> fetch_request_job_;
  base::WeakPtrFactory<ArcRobotAuthCodeFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcRobotAuthCodeFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ROBOT_AUTH_CODE_FETCHER_H_
