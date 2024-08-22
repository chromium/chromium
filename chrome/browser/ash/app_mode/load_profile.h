// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_LOAD_PROFILE_H_
#define CHROME_BROWSER_ASH_APP_MODE_LOAD_PROFILE_H_

#include <memory>
#include <variant>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chromeos/ash/components/login/auth/login_performer.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"

class Profile;

namespace ash::kiosk {

// The final result of a `LoadProfile` call. Includes the `Profile` on success
// or the error reason on failure.
using LoadProfileResult = base::expected<Profile*, KioskAppLaunchError::Error>;

// Convenicence alias to declare result callbacks for `LoadProfile`.
using LoadProfileResultCallback =
    base::OnceCallback<void(LoadProfileResult result)>;

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
    LoadProfileResultCallback on_done);

// Convenience alias to declare references to `LoadProfile`. Useful for callers
// to override `LoadProfile` in tests.
using LoadProfileCallback = base::OnceCallback<decltype(LoadProfile)>;

// Represents the cryptohome mounted state.
enum class CryptohomeMountState { kMounted, kNotMounted, kServiceUnavailable };

// Convenience alias to declare callbacks to the cryptohome mount state.
using CryptohomeMountStateCallback =
    base::OnceCallback<void(CryptohomeMountState result)>;

// Convenience alias to declare functions that check cryptohome mount state.
using CheckCryptohomeCallback =
    base::OnceCallback<std::unique_ptr<CancellableJob>(
        CryptohomeMountStateCallback callback)>;

// Possible errors when performin signin.
enum class PerformSigninError { kPolicyLoadFailed, kAllowlistCheckFailed };

// The final result of a perform signin operation.
using PerformSigninResult =
    base::expected<UserContext, std::variant<PerformSigninError, AuthFailure>>;

// Convenience alias to declare callbacks to `PerformSigninResult`.
using PerformSigninResultCallback =
    base::OnceCallback<void(PerformSigninResult result)>;

// Convenience alias to declare functions that perform signin.
using PerformSigninCallback =
    base::OnceCallback<std::unique_ptr<CancellableJob>(
        KioskAppType app_type,
        AccountId account_id,
        PerformSigninResultCallback callback)>;

// Convenience alias to declare callbacks to the signin `Profile`.
using StartSessionResultCallback = base::OnceCallback<void(Profile& result)>;

// Convenience alias to declare functions that start the session.
using StartSessionCallback = base::OnceCallback<std::unique_ptr<CancellableJob>(
    const UserContext& user_context,
    StartSessionResultCallback on_done)>;

// Same as `LoadProfile` above but allows callers to replace the sub-callbacks
// it executes. Useful in tests.
[[nodiscard]] std::unique_ptr<CancellableJob> LoadProfileWithCallbacks(
    const AccountId& app_account_id,
    KioskAppType app_type,
    CheckCryptohomeCallback check_cryptohome,
    PerformSigninCallback perform_signin,
    StartSessionCallback start_session,
    LoadProfileResultCallback on_done);

}  // namespace ash::kiosk

#endif  // CHROME_BROWSER_ASH_APP_MODE_LOAD_PROFILE_H_
