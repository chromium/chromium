// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_AUTH_ARC_ROBOT_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_ASH_ARC_AUTH_ARC_ROBOT_AUTH_CODE_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/auth/arc_auth_code_fetcher.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace policy {
struct DMServerJobResult;
}

namespace arc {

// This class is responsible to fetch auth code for robot account. Robot auth
// code is used for creation an account on Android side in ARC kiosk mode.
class ArcRobotAuthCodeFetcher : public ArcAuthCodeFetcher {
 public:
  ArcRobotAuthCodeFetcher();

  ArcRobotAuthCodeFetcher(const ArcRobotAuthCodeFetcher&) = delete;
  ArcRobotAuthCodeFetcher& operator=(const ArcRobotAuthCodeFetcher&) = delete;

  ~ArcRobotAuthCodeFetcher() override;

  // ArcAuthCodeFetcher:
  void Fetch(FetchCallback callback) override;

 private:
  void OnFetchRobotAuthCodeCompleted(FetchCallback callback,
                                     policy::DMServerJobResult result);

  std::unique_ptr<policy::DeviceManagementService::Job> fetch_request_job_;
  base::WeakPtrFactory<ArcRobotAuthCodeFetcher> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_AUTH_ARC_ROBOT_AUTH_CODE_FETCHER_H_
