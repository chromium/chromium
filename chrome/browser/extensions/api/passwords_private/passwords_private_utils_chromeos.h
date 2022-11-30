// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_CHROMEOS_H_

#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/password_manager/core/browser/reauth_purpose.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class Profile;

namespace extensions {

// Returns the lifetime of an auth token for a given |purpose|.
base::TimeDelta GetAuthTokenLifetimeForPurpose(
    password_manager::ReauthPurpose purpose);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns whether |profile| has been authorized for password access, and
// whether the auth token is no older than |auth_token_lifetime|. Authorization
// is automatic if no password is needed.
bool IsOsReauthAllowedAsh(Profile* profile,
                          base::TimeDelta auth_token_lifetime);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Makes a crosapi call to determine whether the active user profile has been
// authorized for password access, and whether the auth token is no older than a
// |purpose|-dependent lifetime. The results are asynchronously passed to
// |callback|.
void IsOsReauthAllowedLacrosAsync(
    password_manager::ReauthPurpose purpose,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback callback);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_CHROMEOS_H_
