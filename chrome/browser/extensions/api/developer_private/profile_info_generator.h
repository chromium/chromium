// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_PROFILE_INFO_GENERATOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_PROFILE_INFO_GENERATOR_H_

#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace extensions {

// Creates ProfileInfo from Profile.
api::developer_private::ProfileInfo CreateProfileInfo(Profile* profile);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_PROFILE_INFO_GENERATOR_H_
