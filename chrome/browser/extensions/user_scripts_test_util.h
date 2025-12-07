// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPTS_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPTS_TEST_UTIL_H_

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace extensions::user_scripts_test_util {

// Sets the userScripts API to be allowed or disallowed for the given extension
// in the given profile.
void SetUserScriptsAPIAllowed(Profile* profile,
                              const ExtensionId& extension_id,
                              bool allowed);

}  // namespace extensions::user_scripts_test_util

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPTS_TEST_UTIL_H_
