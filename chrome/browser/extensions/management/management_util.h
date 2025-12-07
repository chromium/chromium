// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MANAGEMENT_MANAGEMENT_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_MANAGEMENT_MANAGEMENT_UTIL_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/management/management_service.h"

namespace extensions {

// Returns the higher of two values - trust level for the machine or that of
// the user profile.
policy::ManagementAuthorityTrustworthiness
GetHigherManagementAuthorityTrustworthiness(Profile* profile);

}  // namespace extensions
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#endif  // CHROME_BROWSER_EXTENSIONS_MANAGEMENT_MANAGEMENT_UTIL_H_
