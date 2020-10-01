// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/android_management_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace arc {

class ArcAndroidManagementChecker : public signin::IdentityManager::Observer {
 public:
  ArcAndroidManagementChecker(Profile* profile, bool retry_on_error);
  ~ArcAndroidManagementChecker() override;

  static void StartClient();

  // Starts the check. On completion |callback| will be invoked with the
  // |result|. This must not be called if there is inflight check.
  // If the instance is destructed while it has inflight check, then the
  // check will be cancelled and |callback| will not be called.
  using CheckCallback =
      base::Callback<void(policy::AndroidManagementClient::Result result)>;
  void StartCheck(const CheckCallback& callback);

 private:
  void StartCheckInternal();
  void OnAndroidManagementChecked(
      policy::AndroidManagementClient::Result result);
  void ScheduleRetry();

  // Ensures the refresh token is loaded in the |identity_manager|.
  void EnsureRefreshTokenLoaded();

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokensLoaded() override;

  // Unowned pointers.
  Profile* profile_;
  signin::IdentityManager* const identity_manager_;

  const CoreAccountId device_account_id_;

  // If true, on error, instead of reporting the error to the caller, schedule
  // the retry with delay.
  const bool retry_on_error_;

  // Keeps current retry delay.
  base::TimeDelta retry_delay_;

  policy::AndroidManagementClient android_management_client_;

  // The callback for the inflight operation.
  CheckCallback callback_;

  base::WeakPtrFactory<ArcAndroidManagementChecker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcAndroidManagementChecker);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_
