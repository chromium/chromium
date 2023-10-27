// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_CHROMEOS_H_

#include "base/time/time.h"
#include "build/chromeos_buildflags.h"

class Profile;

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns whether |profile| has been authorized for password access, and
// whether the auth token is no older than |auth_token_lifetime|. Authorization
// is automatic if no password is needed.
bool IsOsReauthAllowedAsh(Profile* profile,
                          base::TimeDelta auth_token_lifetime);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PASSWORDS_PRIVATE_PASSWORDS_PRIVATE_UTILS_CHROMEOS_H_
