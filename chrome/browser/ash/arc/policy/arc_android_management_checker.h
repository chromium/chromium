// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/arc/android_management_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace arc {

// ArcAndroidManagementChecker checks if Android management is enabled for the
// user while not being a managed ChromeOS user. ARC is disabled if these
// conditions are met (b/26848810).
class ArcAndroidManagementChecker : public signin::IdentityManager::Observer {
 public:
  enum class CheckResult {
    ALLOWED,     // It's OK to enable ARC for the user.
    DISALLOWED,  // The user shouldn't use ARC.
    ERROR,       // There was an error.
  };

  ArcAndroidManagementChecker(Profile* profile,
                              signin::IdentityManager* identity_manager,
                              const CoreAccountId& device_account_id,
                              bool retry_on_error,
                              std::unique_ptr<policy::AndroidManagementClient>
                                  android_management_client);

  ArcAndroidManagementChecker(const ArcAndroidManagementChecker&) = delete;
  ArcAndroidManagementChecker& operator=(const ArcAndroidManagementChecker&) =
      delete;

  ~ArcAndroidManagementChecker() override;

  // Starts the check. On completion |callback| will be invoked with the
  // |result|. This must not be called if there is inflight check.
  // If the instance is destructed while it has inflight check, then the
  // check will be cancelled and |callback| will not be called.
  using CheckCallback = base::OnceCallback<void(CheckResult result)>;
  void StartCheck(CheckCallback callback);

 private:
  void StartCheckInternal();
  void OnAndroidManagementChecked(
      policy::AndroidManagementClient::Result management_result);
  void ScheduleRetry();

  // Ensures the refresh token is loaded in the |identity_manager|.
  void EnsureRefreshTokenLoaded();

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokensLoaded() override;

  // Unowned pointers.
  raw_ptr<Profile> profile_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  const CoreAccountId device_account_id_;

  // If true, on error, instead of reporting the error to the caller, schedule
  // the retry with delay.
  const bool retry_on_error_;

  // Keeps current retry delay.
  base::TimeDelta retry_delay_;

  std::unique_ptr<policy::AndroidManagementClient> android_management_client_;

  // The callback for the inflight operation.
  CheckCallback callback_;

  base::WeakPtrFactory<ArcAndroidManagementChecker> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_POLICY_ARC_ANDROID_MANAGEMENT_CHECKER_H_
