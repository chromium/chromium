// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "components/account_id/account_id.h"

class Profile;

namespace ash {

class UserContext;

// Helper class that implements `LoadProfile()`.
class KioskProfileLoader : public CancellableJob {
 public:
  using Result = base::expected<Profile*, KioskAppLaunchError::Error>;
  using ResultCallback = base::OnceCallback<void(Result result)>;

  [[nodiscard]] static std::unique_ptr<CancellableJob> Run(
      const AccountId& app_account_id,
      KioskAppType app_type,
      ResultCallback on_done);

  KioskProfileLoader(const KioskProfileLoader&) = delete;
  KioskProfileLoader& operator=(const KioskProfileLoader&) = delete;
  ~KioskProfileLoader() override;

 private:
  KioskProfileLoader(const AccountId& app_account_id,
                     KioskAppType app_type,
                     ResultCallback on_done);

  void CheckCryptohomeIsNotMounted();
  void LoginAsKioskAccount();
  void PrepareProfile(const UserContext& user_context);
  void ReturnSuccess(Profile& profile);
  void ReturnError(KioskAppLaunchError::Error result);

  const AccountId account_id_;
  const KioskAppType app_type_;

  std::unique_ptr<CancellableJob> current_step_
      GUARDED_BY_CONTEXT(sequence_checker_);
  ResultCallback on_done_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

// Loads the Kiosk profile for a given app.
//
// It executes the following steps:
//
// 1. Wait for cryptohome and verify cryptohome is not yet mounted.
// 2. Login with the account generated for the Kiosk app.
// 3. Prepare a `Profile` for the app.
//
// `on_done` will either be called with the resulting profile on success, or
// with a `KioskAppLaunchError::Error` on error.
//
// The returned `unique_ptr` can be destroyed to cancel this task. In that case
// `on_done` will not be called.
[[nodiscard]] std::unique_ptr<CancellableJob> LoadProfile(
    const AccountId& app_account_id,
    KioskAppType app_type,
    KioskProfileLoader::ResultCallback on_done);

// Convenience define to declare references to `LoadProfile`. Useful for callers
// to override `LoadProfile` in tests.
using LoadProfileCallback = base::OnceCallback<decltype(LoadProfile)>;

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOADER_H_
