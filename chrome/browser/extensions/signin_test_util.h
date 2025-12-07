// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SIGNIN_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_SIGNIN_TEST_UTIL_H_

#include <optional>
#include <string>

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;
struct AccountInfo;

namespace signin {

class IdentityTestEnvironment;

}  // namespace signin

namespace extensions::signin_test_util {

// Simulates an explicit sign in through the extension installed bubble. This
// should also flip the pref which records an explicit sign in.
AccountInfo SimulateExplicitSignIn(
    Profile* profile,
    signin::IdentityTestEnvironment* identity_test_env,
    std::optional<std::string> email = std::nullopt);

}  // namespace extensions::signin_test_util

#endif  // CHROME_BROWSER_EXTENSIONS_SIGNIN_TEST_UTIL_H_
